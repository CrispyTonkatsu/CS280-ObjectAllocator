/**
 * \file ObjectAllocator.cpp
 * \author Edgar Jose Donoso Mansilla (e.donosomansilla)
 * \course CS280
 * \term Spring 2025
 *
 * \brief Implementation for a basic memory manager
 */

// TODO: Ask if the code needs to be documented both in header and implementation
// TODO: Ask if we are allowed to use strlen and strcpy
// TODO: fix valgrind in test 7

#include "ObjectAllocator.h"
#include <cstddef>
#include <cstdint>
#include <cstring>

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;

static_assert(sizeof(u16) == 2, "uint16_t is not of size 2 bytes");
static_assert(sizeof(u32) == 4, "uint32_t is not of size 4 bytes");

ObjectAllocator::ObjectAllocator(size_t ObjectSize, const OAConfig &config) :
    page_list(nullptr), free_objects_list(nullptr), object_size(ObjectSize), config(config), block_size(0),
    page_size(0), stats() {
  this->config.LeftAlignSize_ = static_cast<unsigned>(calculate_left_alignment_size());
  this->config.InterAlignSize_ = static_cast<unsigned>(calculate_inter_alignment_size());

  // This needs to be called after the alignment data is calculated.
  block_size = calculate_block_size();
  page_size = calculate_page_size();

  stats.ObjectSize_ = ObjectSize;
  stats.PageSize_ = page_size;

  page_push_front(allocate_page());
}

ObjectAllocator::~ObjectAllocator() {
  GenericObject *current_page = page_pop_front();

  while (current_page != nullptr) {
    delete[] reinterpret_cast<u8 *>(current_page);
    current_page = page_pop_front();
  }
}

void *ObjectAllocator::Allocate(const char *label) {
  GenericObject *output = nullptr;

  if (config.UseCPPMemManager_) {
    output = cpp_mem_manager_allocate();

  } else {
    output = custom_mem_manager_allocate(label);
  }

  stats.Allocations_++;
  stats.ObjectsInUse_++;

  if (stats.ObjectsInUse_ > stats.MostObjects_) {
    stats.MostObjects_ = stats.ObjectsInUse_;
  }

  return output;
}

void ObjectAllocator::Free(void *Object) {
  if (config.UseCPPMemManager_) {
    cpp_mem_manager_free(Object);
  } else {
    custom_mem_manager_free(Object);
  }

  stats.Deallocations_++;
  stats.ObjectsInUse_--;
}

unsigned ObjectAllocator::DumpMemoryInUse(DUMPCALLBACK fn) const {
  GenericObject *current_page = page_list;
  unsigned in_use_count = 0;

  while (current_page != nullptr) {
    u8 *object = reinterpret_cast<u8 *>(current_page) + sizeof(void *) + config.LeftAlignSize_ +
                 config.HBlockInfo_.size_ + config.PadBytes_;

    for (size_t i = 0; i < config.ObjectsPerPage_; i++) {
      if (!object_check_is_free(reinterpret_cast<GenericObject *>(object))) {
        fn(object, object_size);
        in_use_count++;
      }

      object += block_size;
    }

    current_page = current_page->Next;
  }

  return in_use_count;
}

unsigned ObjectAllocator::ValidatePages(VALIDATECALLBACK fn) const {
  if (!config.DebugOn_ || config.PadBytes_ == 0) {
    return 0;
  }

  GenericObject *current_page = page_list;
  unsigned in_use_count = 0;

  while (current_page != nullptr) {
    u8 *object = reinterpret_cast<u8 *>(current_page) + sizeof(void *) + config.LeftAlignSize_ +
                 config.HBlockInfo_.size_ + config.PadBytes_;

    for (size_t i = 0; i < config.ObjectsPerPage_; i++) {
      if (!object_validate_padding(reinterpret_cast<GenericObject *>(object))) {
        fn(object, object_size);
        in_use_count++;
      }

      object += block_size;
    }

    current_page = current_page->Next;
  }

  return in_use_count;
}

// TODO: Implement this for full extra credit
unsigned ObjectAllocator::FreeEmptyPages() { return 0; }

bool ObjectAllocator::ImplementedExtraCredit() { return true; }

void ObjectAllocator::SetDebugState(bool State) { config.DebugOn_ = State; }

const void *ObjectAllocator::GetFreeList() const { return free_objects_list; }

const void *ObjectAllocator::GetPageList() const { return page_list; }

OAConfig ObjectAllocator::GetConfig() const { return config; }

OAStats ObjectAllocator::GetStats() const { return stats; }

GenericObject *ObjectAllocator::cpp_mem_manager_allocate() {
  u8 *new_object = nullptr;
  try {
    new_object = new u8[page_size];

  } catch (const std::bad_alloc &) {
    throw OAException(OAException::E_NO_MEMORY, "Bad allocation thrown by 'new' operator.");
  }

  return reinterpret_cast<GenericObject *>(new_object);
}

void ObjectAllocator::cpp_mem_manager_free(void *object) { delete[] static_cast<u8 *>(object); }

GenericObject *ObjectAllocator::custom_mem_manager_allocate(const char *label) {
  if (free_objects_list == nullptr) {
    page_push_front(allocate_page());
  }

  GenericObject *output = object_pop_front();
  header_update_alloc(output, label);

  return output;
}

void ObjectAllocator::custom_mem_manager_free(void *object) {

  GenericObject *cast_object = static_cast<GenericObject *>(object);

  if (config.DebugOn_) {
    if (!object_validate_location(cast_object)) {
      throw OAException(
          OAException::E_BAD_BOUNDARY, "The memory address lies outside of the allocated blocks' boundaries");
    }

    if (object_check_is_free(cast_object)) {
      throw OAException(OAException::E_MULTIPLE_FREE, "The object is being deallocated multiple times");
    }

    if (!object_validate_padding(cast_object)) {
      throw OAException(
          OAException::E_CORRUPTED_BLOCK,
          "The object's padding bytes have been corrupted, check pointer math in your code");
    }
  }

  header_update_dealloc(cast_object);
  object_push_front(cast_object, FREED_PATTERN);
}

void ObjectAllocator::object_push_front(GenericObject *object, const unsigned char signature) {
  if (object == nullptr) {
    return;
  }

  // TODO: Implement inter alignment signature writing
  write_signature(object, signature, object_size);

  // Writing padding
  u8 *raw_object = reinterpret_cast<u8 *>(object);
  if (config.PadBytes_ > 0) {
    write_signature(raw_object - config.PadBytes_, PAD_PATTERN, config.PadBytes_);
    write_signature(raw_object + object_size, PAD_PATTERN, config.PadBytes_);
  }

  object->Next = free_objects_list;
  free_objects_list = object;

  stats.FreeObjects_++;
}

GenericObject *ObjectAllocator::object_pop_front() {
  if (free_objects_list == nullptr) {
    return nullptr;
  }

  GenericObject *output = free_objects_list;
  free_objects_list = free_objects_list->Next;

  write_signature(output, ALLOCATED_PATTERN, object_size);

  stats.FreeObjects_--;
  return output;
}

GenericObject *ObjectAllocator::allocate_page() {
  if (config.MaxPages_ != 0 && stats.PagesInUse_ + 1 > config.MaxPages_) {
    throw OAException(OAException::E_NO_PAGES, "The maximum amount of pages has been allocated");
  }

  u8 *new_page = nullptr;
  try {
    new_page = new u8[page_size];

  } catch (const std::bad_alloc &) {
    throw OAException(OAException::E_NO_MEMORY, "Bad allocation thrown by 'new' operator.");
  }

  GenericObject *new_obj = reinterpret_cast<GenericObject *>(new_page);
  new_obj->Next = nullptr;

  return new_obj;
}

void ObjectAllocator::page_push_front(GenericObject *page) {
  if (page == nullptr) {
    return;
  }

  u8 *raw_page = reinterpret_cast<u8 *>(page);

  u8 *current_block = raw_page + sizeof(void *) + config.LeftAlignSize_ + config.HBlockInfo_.size_ + config.PadBytes_;

  // TODO: Implement left alignment signature writing

  for (size_t i = 0; i < config.ObjectsPerPage_; i++) {
    GenericObject *current_object = reinterpret_cast<GenericObject *>(current_block);

    header_initialize(current_object);
    object_push_front(current_object, UNALLOCATED_PATTERN);

    current_block += block_size;
  }

  page->Next = page_list;
  page_list = page;

  stats.PagesInUse_++;
}

// TODO: Confirm that there are no other things that need to be done in the destructor
GenericObject *ObjectAllocator::page_pop_front() {
  if (page_list == nullptr) {
    return nullptr;
  }

  GenericObject *output = page_list;
  page_list = page_list->Next;

  if (config.HBlockInfo_.type_ == OAConfig::hbExternal) {
    u8 *header_location = reinterpret_cast<u8 *>(output) + sizeof(void *) + config.LeftAlignSize_;
    for (size_t i = 0; i < config.ObjectsPerPage_; i++) {
      MemBlockInfo **header_ptr_ptr = reinterpret_cast<MemBlockInfo **>(header_location);
      header_external_delete(header_ptr_ptr);
      header_location += block_size;
    }
  }

  stats.PagesInUse_--;
  return output;
}

bool ObjectAllocator::object_check_is_free(GenericObject *object) const {
  bool was_freed = false;

  switch (config.HBlockInfo_.type_) {
    case OAConfig::hbNone: was_freed = object_is_in_free_list(object); break;

    case OAConfig::hbBasic: {
      u8 *reading_location = reinterpret_cast<u8 *>(object) - config.PadBytes_ - config.HBlockInfo_.size_;
      u8 *flag = reading_location + sizeof(u32);
      was_freed = (*flag) == 0;
    } break;

    case OAConfig::hbExtended: {
      u8 *reading_location = reinterpret_cast<u8 *>(object) - config.PadBytes_ - config.HBlockInfo_.size_;
      reading_location += config.HBlockInfo_.additional_;
      reading_location += sizeof(u16);
      u8 *flag = reading_location + sizeof(u32);
      was_freed = (*flag) == 0;
    } break;

    case OAConfig::hbExternal: {
      u8 *reading_location = reinterpret_cast<u8 *>(object) - config.PadBytes_ - config.HBlockInfo_.size_;

      MemBlockInfo **header_ptr = reinterpret_cast<MemBlockInfo **>(reading_location);
      was_freed = (*header_ptr) == nullptr;
    } break;
  }

  // TODO: Consider whether this should be run for all cases or just no headers (user corruptions could lead to double
  // free if they change the header)
  /*was_freed = object_is_in_free_list(object);*/

  return was_freed;
}

bool ObjectAllocator::object_is_in_free_list(GenericObject *object) const {
  GenericObject *current_object = free_objects_list;

  while (current_object != nullptr) {
    if (object == current_object) {
      return true;
    }

    current_object = current_object->Next;
  }

  return false;
}

bool ObjectAllocator::object_validate_location(GenericObject *location) const {
  if (location == nullptr) {
    return false;
  }

  GenericObject *page = object_is_inside_page(location);
  if (page == nullptr) {
    return false;
  }

  u8 *raw_location = reinterpret_cast<u8 *>(location);
  u8 *blocks_start = reinterpret_cast<u8 *>(page) +
                     (sizeof(void *) + config.LeftAlignSize_ + config.HBlockInfo_.size_ + config.PadBytes_);

  ptrdiff_t distance = raw_location - blocks_start;
  if (distance < 0) {
    return false;
  }

  return static_cast<size_t>(distance) % block_size == 0;
}

GenericObject *ObjectAllocator::object_is_inside_page(GenericObject *object) const {
  GenericObject *current_page = page_list;

  while (current_page != nullptr) {
    if (is_in_range(reinterpret_cast<u8 *>(current_page), page_size, reinterpret_cast<u8 *>(object))) {
      return current_page;
    }

    current_page = current_page->Next;
  }

  return nullptr;
}

bool ObjectAllocator::object_validate_padding(GenericObject *object) const {
  if (config.PadBytes_ == 0) {
    return true;
  }

  u8 *left_pad_start = reinterpret_cast<u8 *>(object) - config.PadBytes_;
  u8 *right_pad_start = reinterpret_cast<u8 *>(object) + object_size;
  for (size_t i = 0; i < config.PadBytes_; i++) {
    u8 left_pad = *(left_pad_start + i);
    u8 right_pad = *(right_pad_start + i);

    if (left_pad != PAD_PATTERN || right_pad != PAD_PATTERN) {
      return false;
    }
  }

  return true;
}

void ObjectAllocator::header_initialize(GenericObject *block_location) {
  switch (config.HBlockInfo_.type_) {
    case OAConfig::hbNone: break;
    case OAConfig::hbBasic: header_basic_initialize(block_location); break;
    case OAConfig::hbExtended: header_extended_initialize(block_location); break;
    case OAConfig::hbExternal: header_external_initialize(block_location); break;
    default: break;
  }
}

void ObjectAllocator::header_basic_initialize(GenericObject *block_location) {
  u8 *writing_location = reinterpret_cast<u8 *>(block_location) - config.PadBytes_ - config.HBlockInfo_.size_;

  u32 *allocation_number = reinterpret_cast<u32 *>(writing_location);
  *allocation_number = 0;

  u8 *flag = writing_location + sizeof(u32);
  (*flag) = 0;
}

void ObjectAllocator::header_extended_initialize(GenericObject *block_location) {
  u8 *writing_location = reinterpret_cast<u8 *>(block_location) - config.PadBytes_ - config.HBlockInfo_.size_;

  memset(writing_location, 0, config.HBlockInfo_.additional_);

  writing_location += config.HBlockInfo_.additional_;
  u16 *use_counter = reinterpret_cast<u16 *>(writing_location);
  (*use_counter) = 0;

  writing_location += sizeof(u16);
  u32 *allocation_number = reinterpret_cast<u32 *>(writing_location);
  (*allocation_number) = 0;

  writing_location += sizeof(u32);
  u8 *flag = writing_location;
  (*flag) = 0;
}

void ObjectAllocator::header_external_initialize(GenericObject *block_location) {
  u8 *writing_location = reinterpret_cast<u8 *>(block_location) - config.PadBytes_ - config.HBlockInfo_.size_;

  MemBlockInfo **header_ptr = reinterpret_cast<MemBlockInfo **>(writing_location);
  *header_ptr = nullptr;
}

void ObjectAllocator::header_update_alloc(GenericObject *block_location, const char *label) {
  switch (config.HBlockInfo_.type_) {
    case OAConfig::hbNone: break;
    case OAConfig::hbBasic: header_basic_update_alloc(block_location); break;
    case OAConfig::hbExtended: header_extended_update_alloc(block_location); break;
    case OAConfig::hbExternal: header_external_update_alloc(block_location, label); break;
    default: break;
  }
}

void ObjectAllocator::header_basic_update_alloc(GenericObject *block_location) {
  u8 *writing_location = reinterpret_cast<u8 *>(block_location) - config.PadBytes_ - config.HBlockInfo_.size_;

  u32 *allocation_number = reinterpret_cast<u32 *>(writing_location);
  (*allocation_number) = static_cast<u32>(stats.Allocations_ + 1);

  u8 *flag = writing_location + sizeof(u32);
  (*flag) |= 1;
}

void ObjectAllocator::header_extended_update_alloc(GenericObject *block_location) {
  u8 *writing_location = reinterpret_cast<u8 *>(block_location) - config.PadBytes_ - config.HBlockInfo_.size_;

  writing_location += config.HBlockInfo_.additional_;
  u16 *use_counter = reinterpret_cast<u16 *>(writing_location);
  (*use_counter)++;

  writing_location += sizeof(u16);
  u32 *allocation_number = reinterpret_cast<u32 *>(writing_location);
  (*allocation_number) = static_cast<u32>(stats.Allocations_ + 1);

  writing_location += sizeof(u32);
  u8 *flag = writing_location;
  (*flag) |= 1;
}

void ObjectAllocator::header_external_update_alloc(GenericObject *block_location, const char *label) {
  u8 *writing_location = reinterpret_cast<u8 *>(block_location) - config.PadBytes_ - config.HBlockInfo_.size_;

  MemBlockInfo **header_ptr_ptr = reinterpret_cast<MemBlockInfo **>(writing_location);
  *header_ptr_ptr = new MemBlockInfo;

  (*header_ptr_ptr)->in_use = true;
  (*header_ptr_ptr)->alloc_num = stats.Allocations_ + 1;

  if (label != nullptr) {
    char *label_copy = new char[strlen(label) + 1];
    strcpy(label_copy, label);
    (*header_ptr_ptr)->label = label_copy;
  } else {
    (*header_ptr_ptr)->label = nullptr;
  }
}

void ObjectAllocator::header_update_dealloc(GenericObject *block_location) {
  switch (config.HBlockInfo_.type_) {
    case OAConfig::hbNone: break;
    case OAConfig::hbBasic: header_basic_update_dealloc(block_location); break;
    case OAConfig::hbExtended: header_extended_update_dealloc(block_location); break;
    case OAConfig::hbExternal: header_external_update_dealloc(block_location); break;
    default: break;
  }
}

void ObjectAllocator::header_basic_update_dealloc(GenericObject *block_location) {
  u8 *writing_location = reinterpret_cast<u8 *>(block_location) - config.PadBytes_ - config.HBlockInfo_.size_;

  u32 *allocation_number = reinterpret_cast<u32 *>(writing_location);
  (*allocation_number) = 0;

  u8 *flag = writing_location + sizeof(u32);
  (*flag) &= ~1;
}

void ObjectAllocator::header_extended_update_dealloc(GenericObject *block_location) {
  u8 *writing_location = reinterpret_cast<u8 *>(block_location) - config.PadBytes_ - config.HBlockInfo_.size_;

  writing_location += config.HBlockInfo_.additional_ + sizeof(u16);
  u32 *allocation_number = reinterpret_cast<u32 *>(writing_location);
  (*allocation_number) = 0;

  writing_location += sizeof(u32);
  u8 *flag = writing_location;
  (*flag) &= ~1;
}

void ObjectAllocator::header_external_update_dealloc(GenericObject *block_location) {
  u8 *writing_location = reinterpret_cast<u8 *>(block_location) - config.PadBytes_ - config.HBlockInfo_.size_;

  MemBlockInfo **header_ptr_ptr = reinterpret_cast<MemBlockInfo **>(writing_location);
  header_external_delete(header_ptr_ptr);
}

void ObjectAllocator::header_external_delete(MemBlockInfo **header_ptr_ptr) {
  if (header_ptr_ptr == nullptr || *header_ptr_ptr == nullptr) {
    return;
  }

  MemBlockInfo *header_ptr = *header_ptr_ptr;
  if (header_ptr->label != nullptr) {
    delete[] header_ptr->label;
  }
  delete header_ptr;

  *header_ptr_ptr = nullptr;
}

size_t ObjectAllocator::get_header_size(OAConfig::HeaderBlockInfo info) const {
  switch (info.type_) {
    case OAConfig::hbNone: return 0;
    case OAConfig::hbBasic: return OAConfig::BASIC_HEADER_SIZE;
    case OAConfig::hbExtended: return info.size_;
    case OAConfig::hbExternal: return sizeof(void *);
    default: return 0;
  }
}

size_t ObjectAllocator::calculate_left_alignment_size() const {
  if (config.Alignment_ <= 0) return 0;
  size_t remainder = (sizeof(void *) + config.PadBytes_ + get_header_size(config.HBlockInfo_)) % config.Alignment_;
  return (remainder > 0) ? config.Alignment_ - remainder : 0;
}

size_t ObjectAllocator::calculate_inter_alignment_size() const {
  if (config.Alignment_ <= 0) return 0;
  size_t remainder = block_size % config.Alignment_;
  return (remainder > 0) ? config.Alignment_ - remainder : 0;
}

size_t ObjectAllocator::calculate_block_size() const {
  return get_header_size(config.HBlockInfo_) + (2 * config.PadBytes_) + object_size + config.InterAlignSize_;
}

size_t ObjectAllocator::calculate_page_size() const {
  size_t total = sizeof(void *) + config.LeftAlignSize_;
  total += config.ObjectsPerPage_ * block_size;
  total += (config.ObjectsPerPage_ - 1) * config.InterAlignSize_;

  return total;
}

void ObjectAllocator::write_signature(GenericObject *object, const unsigned char pattern, size_t size) {
  if (object == nullptr || !config.DebugOn_) {
    return;
  }

  memset(object, pattern, size);
}

void ObjectAllocator::write_signature(u8 *location, const unsigned char pattern, size_t size) {
  if (location == nullptr || !config.DebugOn_) {
    return;
  }

  memset(location, pattern, size);
}

bool ObjectAllocator::is_in_range(u8 *start, size_t length, u8 *address) const {
  ptrdiff_t offset = address - start;
  return 0 < offset && offset < static_cast<intptr_t>(length);
}
