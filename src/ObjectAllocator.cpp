/**
 * @file ObjectAllocator.cpp
 * @author Edgar Jose Donoso Mansilla (e.donosomansilla)
 * @course CS280
 * @term Spring 2025
 *
 * @brief Implementation for a basic memory manager
 */

// Reminders:
// - OAConfig is read only
// - Stats is meant to be updated by the dev
// - static_cast only for void*, reinterpret_cast all other T

// TODO: Document all code

// TODO: Testing:
// - Memory Allocation & Deletion
// - Memory Alignment + Padding & Headers
// - Correct Debug & Stats info

// TODO: Check that all the debug only features don't trigger when debug is off

// TODO: Make sure that all exception messages match what is in the handout requirements

// TODO: Check that there are no STL usages
#include "ObjectAllocator.h"
#include <cstdint>
#include <cstring>

using u8 = uint8_t;

ObjectAllocator::ObjectAllocator(size_t ObjectSize, const OAConfig &config) :
    page_list(nullptr), free_objects_list(nullptr), object_size(ObjectSize), config(config),
    block_size(calculate_block_size()) {
  this->config.LeftAlignSize_ = static_cast<unsigned>(calculate_left_alignment_size());
  this->config.InterAlignSize_ = static_cast<unsigned>(calculate_inter_alignment_size());

  // TODO: put this in the initializer list
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
  // TODO: Add debug checks

  GenericObject *output = nullptr;

  if (config.UseCPPMemManager_) {
    output = cpp_mem_manager_allocate(label);

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
  // TODO: Add debug checks

  if (config.UseCPPMemManager_) {
    cpp_mem_manager_free(Object);
  } else {
    custom_mem_manager_free(Object);
  }

  stats.Deallocations_++;
  stats.ObjectsInUse_--;
}

unsigned ObjectAllocator::DumpMemoryInUse(DUMPCALLBACK) const { return 0; }

unsigned ObjectAllocator::ValidatePages(VALIDATECALLBACK) const { return 0; }

unsigned ObjectAllocator::FreeEmptyPages() { return 0; }

bool ObjectAllocator::ImplementedExtraCredit() {
  // TODO: Implement the extra credit and change this.
  return false;
}

void ObjectAllocator::SetDebugState(bool State) { config.DebugOn_ = State; }

const void *ObjectAllocator::GetFreeList() const { return free_objects_list; }

const void *ObjectAllocator::GetPageList() const { return page_list; }

OAConfig ObjectAllocator::GetConfig() const { return config; }

OAStats ObjectAllocator::GetStats() const { return stats; }

GenericObject *ObjectAllocator::cpp_mem_manager_allocate(const char *) { return nullptr; }

void ObjectAllocator::cpp_mem_manager_free(void *) {}

GenericObject *ObjectAllocator::custom_mem_manager_allocate(const char *) {
  // TODO: No idea what the label is supposed to do.

  if (free_objects_list == nullptr) {
    page_push_front(allocate_page());
  }

  return object_pop_front();
}

void ObjectAllocator::custom_mem_manager_free(void *object) {
  // TODO: Add the debug checks

  // HACK: Left off here.

  // TODO: Double Free:
  // - For no headers -> that the pointer is not inside any of the free_object_list data space.
  // - For headers -> just check the flag for in use.
  //
  // TODO: Boundary Checks:
  // Ask how to do the boundary checks.
  // - For no headers -> check that the pointer is inside a page
  // - For padding ->
  // - For headers ->
  // - For alignment -> offset % alignment == 0

  // TODO: Implement more thorough delete which will update the header and such
  GenericObject *cast_object = reinterpret_cast<GenericObject *>(object);
  object_push_front(cast_object, FREED_PATTERN);
}

void ObjectAllocator::object_push_front(GenericObject *object, const unsigned char signature) {
  if (object == nullptr) {
    return;
  }

  // TODO: Probably add the signing and other writing for the object here.
  write_signature(object, signature, object_size);

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

  if (config.DebugOn_) {
    // TODO: Implement a signing function (debug patterns)
  }

  for (size_t i = 0; i < config.ObjectsPerPage_; i++) {
    GenericObject *current_object = reinterpret_cast<GenericObject *>(current_block);
    object_push_front(current_object, UNALLOCATED_PATTERN);

    current_block += block_size;
  }

  page->Next = page_list;
  page_list = page;

  stats.PagesInUse_++;
}

// TODO: Make sure to implement more checking before just giving a page away.
GenericObject *ObjectAllocator::page_pop_front() {
  if (page_list == nullptr) {
    return nullptr;
  }

  GenericObject *output = page_list;
  page_list = page_list->Next;

  stats.PagesInUse_--;
  return output;
}

bool ObjectAllocator::object_check_double_free(GenericObject *object) const {
  bool was_freed = false;

  switch (config.HBlockInfo_.type_) {
    case OAConfig::hbNone: was_freed = object_is_in_free_list(object); break;
    case OAConfig::hbBasic: break;
    case OAConfig::hbExtended: break;
    case OAConfig::hbExternal: break;
  }

  // TODO: Consider whether this should be run for all cases or just no headers (user corruptions could lead to double
  // free if they change the header)
  /*was_freed = object_is_in_free_list(object);*/

  return was_freed;
}

bool ObjectAllocator::object_is_in_free_list(GenericObject *object) const {
  GenericObject *current_object = free_objects_list;

  while (current_object != nullptr) {
    // TODO: Ask whether we need to also check that object is not pointing to somewhere inside the block
    if (object == current_object) {
      return true;
    }

    current_object = current_object->Next;
  }

  return false;
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
  return get_header_size(config.HBlockInfo_) + (2 * config.PadBytes_) + object_size;
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
