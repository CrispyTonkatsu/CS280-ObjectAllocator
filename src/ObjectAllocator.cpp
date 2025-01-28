/**
 * \file ObjectAllocator.cpp
 * \author Edgar Jose Donoso Mansilla (e.donosomansilla)
 * \course CS280
 * \term Spring 2025
 *
 * \brief Implementation for a basic memory manager
 */

// TODO: Copy the documentation to here

#include "ObjectAllocator.h"
#include <cstddef>
#include <cstring>

// Alias declaration just for internal use
using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;

static_assert(sizeof(u16) == 2, "uint16_t is not of size 2 bytes");
static_assert(sizeof(u32) == 4, "uint32_t is not of size 4 bytes");

/*!
 * \brief Creates the ObjectManager per the specified values. Throws an exception if the construction fails.
 * (Memory allocation problem)
 *
 * \param ObjectSize The size to allocate for each object
 * \param config The configuration which the allocator will use
 */
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

/*!
 * \brief Destroys the ObjectManager (never throws)
 */
ObjectAllocator::~ObjectAllocator() {
  GenericObject *current_page = page_pop_front();

  while (current_page != nullptr) {
    delete[] reinterpret_cast<u8 *>(current_page);
    current_page = page_pop_front();
  }
}

/*!
 * \brief Take an object from the free list and give it to the client (simulates new). Throws an exception if the
 * object can't be allocated. (Memory allocation problem)
 *
 * \param label The label to put in the external header
 *
 * \return Pointer to the allocated block
 */
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

/*!
 * \brief Returns an object to the free list for the client (simulates delete). Throws an exception if the the object
 * can't be freed. (Invalid object)
 *
 * \param Object Pointer to the block to deallocate
 */
void ObjectAllocator::Free(void *Object) {
  if (config.UseCPPMemManager_) {
    cpp_mem_manager_free(Object);
  } else {
    custom_mem_manager_free(Object);
  }

  stats.Deallocations_++;
  stats.ObjectsInUse_--;
}

/*!
 * \brief Calls the callback fn for each block still in use
 *
 * \param fn Callback to call for each block
 *
 * \return Amount of blocks still in use
 */
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

/*!
 * \brief Calls the callback fn for each block that is potentially corrupted
 *
 * \param fn Callback to call for each block
 *
 * \return Amount of blocks corrupted
 */
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

/*!
 * \brief Frees all empty pages
 */
unsigned ObjectAllocator::FreeEmptyPages() {
  GenericObject *current_page = page_list;
  unsigned deleted = 0;

  while (current_page != nullptr) {
    GenericObject *next_page = current_page->Next;

    u8 *objects_start = reinterpret_cast<u8 *>(current_page) + sizeof(void *) + config.LeftAlignSize_ +
                        config.HBlockInfo_.size_ + config.PadBytes_;

    bool empty_page = true;
    u8 *current_object = objects_start;

    for (size_t i = 0; i < config.ObjectsPerPage_; i++) {
      if (!object_check_is_free(reinterpret_cast<GenericObject *>(current_object))) {
        empty_page = false;
        break;
      }

      current_object += block_size;
    }

    if (empty_page) {
      current_object = objects_start;

      for (size_t i = 0; i < config.ObjectsPerPage_; i++) {
        generic_object_remove(free_objects_list, reinterpret_cast<GenericObject *>(current_object));

        current_object += block_size;
        if (stats.FreeObjects_ > 0) {
          stats.FreeObjects_--;
        }
      }

      generic_object_remove(page_list, current_page);
      delete[] reinterpret_cast<u8 *>(current_page);

      stats.PagesInUse_--;
      deleted++;
    }

    current_page = next_page;
  }

  return deleted;
}

/*!
 * \brief Returns true if FreeEmptyPages and alignments are implemented
 *
 * \return Whether extra credit was implemented
 */
bool ObjectAllocator::ImplementedExtraCredit() { return true; }

/*!
 * \brief Modifies the debug state
 *
 * \param State Whether to enable or disable debug features
 */
void ObjectAllocator::SetDebugState(bool State) { config.DebugOn_ = State; }

/*!
 * \brief Getter for the list of free objects in the allocator
 *
 * \return Pointer to the head of the list
 */
const void *ObjectAllocator::GetFreeList() const { return free_objects_list; }

/*!
 * \brief Getter for the list of pages being used by the allocator
 *
 * \return Pointer to the head of the list
 */
const void *ObjectAllocator::GetPageList() const { return page_list; }

/*!
 * \brief Getter for the configuration of the allocator
 *
 * \return The configuration of the allocator
 */
OAConfig ObjectAllocator::GetConfig() const { return config; }

/*!
 * \brief Getter for the statistics of the allocator
 *
 * \return The statistics of the allocator
 */
OAStats ObjectAllocator::GetStats() const { return stats; }

/*!
 * \brief Use the C++ native memory allocator to allocate an object
 *
 * \return Pointer to the object's location in memory
 */
GenericObject *ObjectAllocator::cpp_mem_manager_allocate() {
  u8 *new_object = nullptr;
  try {
    new_object = new u8[page_size];

  } catch (const std::bad_alloc &) {
    throw OAException(OAException::E_NO_MEMORY, "Bad allocation thrown by 'new' operator.");
  }

  return reinterpret_cast<GenericObject *>(new_object);
}

/*!
 * \brief Use the C++ native memory allocator to free an object from memory
 *
 * \param object Pointer to the object to free
 */
void ObjectAllocator::cpp_mem_manager_free(void *object) { delete[] static_cast<u8 *>(object); }

/*!
 * \brief Use the custom object allocator to allocate an object in memory
 *
 * \return Pointer to the object's location in memory
 */
GenericObject *ObjectAllocator::custom_mem_manager_allocate(const char *label) {
  if (free_objects_list == nullptr) {
    page_push_front(allocate_page());
  }

  GenericObject *output = object_pop_front();
  header_update_alloc(output, label);

  return output;
}

/*!
 * \brief Use the custom memory allocator to free an object from memory
 *
 * \param object Pointer to the object to free
 */
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

/*!
 * \brief Links object in such a way that it is the front of the free object list
 *
 * \param object The object that will be inserted into the linked list
 * \param signature Pattern to sign the space with
 */
void ObjectAllocator::object_push_front(GenericObject *object, const unsigned char signature) {
  if (object == nullptr) {
    return;
  }

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

/*!
 * \brief Returns the first object in the free_objects_list
 *
 * \return The pointer to the object's location
 */
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

/*!
 * \brief Factory method for a page in memory.
 *
 * \return Pointer to allocated page
 */
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

/*!
 * \brief Updates `free_object_list` to include the pointers to the next available blocks. It also adds the page to
 * the `page_list`.
 *
 * \param page The page to add
 */
void ObjectAllocator::page_push_front(GenericObject *page) {
  if (page == nullptr) {
    return;
  }

  u8 *raw_page = reinterpret_cast<u8 *>(page);
  write_signature(page + config.LeftAlignSize_, ALIGN_PATTERN, config.LeftAlignSize_);

  u8 *current_data = raw_page + sizeof(void *) + config.LeftAlignSize_ + config.HBlockInfo_.size_ + config.PadBytes_;

  for (size_t i = 0; i < config.ObjectsPerPage_; i++) {
    GenericObject *current_object = reinterpret_cast<GenericObject *>(current_data);

    header_initialize(current_object);
    object_push_front(current_object, UNALLOCATED_PATTERN);

    if (i + 1 < config.ObjectsPerPage_) {
      write_signature(current_data + object_size + config.PadBytes_, ALIGN_PATTERN, config.InterAlignSize_);
    }

    current_data += block_size;
  }

  page->Next = page_list;
  page_list = page;

  stats.PagesInUse_++;
}

/*!
 * \brief Returns the first page in the list. It will not check if a page has objects in use or not.
 *
 * \return The page at the front of the list
 */
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

/*!
 * \brief Checks if the object is already free
 *
 * \param object The object to check
 * \return Whether the object has already been freed
 */
bool ObjectAllocator::object_check_is_free(GenericObject *object) const {
  bool is_free = false;

  switch (config.HBlockInfo_.type_) {
    case OAConfig::hbNone: is_free = object_is_in_free_list(object); break;

    case OAConfig::hbBasic: {
      u8 *reading_location = reinterpret_cast<u8 *>(object) - config.PadBytes_ - config.HBlockInfo_.size_;
      u8 *flag = reading_location + sizeof(u32);
      is_free = (*flag) == 0;
    } break;

    case OAConfig::hbExtended: {
      u8 *reading_location = reinterpret_cast<u8 *>(object) - config.PadBytes_ - config.HBlockInfo_.size_;
      reading_location += config.HBlockInfo_.additional_;
      reading_location += sizeof(u16);
      u8 *flag = reading_location + sizeof(u32);
      is_free = (*flag) == 0;
    } break;

    case OAConfig::hbExternal: {
      u8 *reading_location = reinterpret_cast<u8 *>(object) - config.PadBytes_ - config.HBlockInfo_.size_;

      MemBlockInfo **header_ptr = reinterpret_cast<MemBlockInfo **>(reading_location);
      is_free = (*header_ptr) == nullptr;
    } break;
  }

  return is_free;
}

/*!
 * \brief Checks if the object is in the free_objects_list
 *
 * \param object The object to look for in the list
 * \return Whether the object is in the list
 */
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

/*!
 * \brief Checks if the pointer is a valid pointer to a block
 *
 * \param location The location to validate
 * \return Whether the object is within a valid location
 */
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

/*!
 * \brief Checks if the object is in any of the allocated pages
 *
 * \param object The object to look for in the pages
 * \return The page in which the object is located
 */
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

/*!
 * \brief Checks if the object padding is corrupted
 *
 * \param object The object to check
 * \return Whether the padding is valid
 */
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

/*!
 * \brief This function will initialize the proper header as defined in the OA's config struct.
 *
 * \param block_location Where the block is located (pointer to start of data)
 */
void ObjectAllocator::header_initialize(GenericObject *block_location) {
  switch (config.HBlockInfo_.type_) {
    case OAConfig::hbNone: break;
    case OAConfig::hbBasic: header_basic_initialize(block_location); break;
    case OAConfig::hbExtended: header_extended_initialize(block_location); break;
    case OAConfig::hbExternal: header_external_initialize(block_location); break;
    default: break;
  }
}

/*!
 * \brief This function will initialize a basic header block
 *
 * \param block_location Where the block is located (pointer to start of data)
 */
void ObjectAllocator::header_basic_initialize(GenericObject *block_location) {
  u8 *writing_location = reinterpret_cast<u8 *>(block_location) - config.PadBytes_ - config.HBlockInfo_.size_;

  u32 *allocation_number = reinterpret_cast<u32 *>(writing_location);
  *allocation_number = 0;

  u8 *flag = writing_location + sizeof(u32);
  (*flag) = 0;
}

/*!
 * \brief This function will initialize an extended header block
 *
 * \param block_location Where the block is located (pointer to start of data)
 */
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

/*!
 * \brief This function will initialize an external header block. This means it will set the header values to nullptr
 *
 * \param block_location Where the block is located (pointer to start of data)
 */
void ObjectAllocator::header_external_initialize(GenericObject *block_location) {
  u8 *writing_location = reinterpret_cast<u8 *>(block_location) - config.PadBytes_ - config.HBlockInfo_.size_;

  MemBlockInfo **header_ptr = reinterpret_cast<MemBlockInfo **>(writing_location);
  *header_ptr = nullptr;
}

/*!
 * \brief This function will update the allocation data of the corresponding header
 *
 * \param block_location Where the block is located (pointer to start of data)
 */
void ObjectAllocator::header_update_alloc(GenericObject *block_location, const char *label) {
  switch (config.HBlockInfo_.type_) {
    case OAConfig::hbNone: break;
    case OAConfig::hbBasic: header_basic_update_alloc(block_location); break;
    case OAConfig::hbExtended: header_extended_update_alloc(block_location); break;
    case OAConfig::hbExternal: header_external_update_alloc(block_location, label); break;
    default: break;
  }
}

/*!
 * \brief This function will update a basic header's data
 *
 * \param block_location Where the block is located (pointer to start of data)
 */
void ObjectAllocator::header_basic_update_alloc(GenericObject *block_location) {
  u8 *writing_location = reinterpret_cast<u8 *>(block_location) - config.PadBytes_ - config.HBlockInfo_.size_;

  u32 *allocation_number = reinterpret_cast<u32 *>(writing_location);
  (*allocation_number) = static_cast<u32>(stats.Allocations_ + 1);

  u8 *flag = writing_location + sizeof(u32);
  (*flag) |= 1;
}

/*!
 * \brief This function will update the extended header due to an allocation
 *
 * \param block_location Where the block is located (pointer to start of data)
 */
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

/*!
 * \brief This function will update the external header due to an allocation. This means it will allocate the header
 * with the corresponding data.
 *
 * \param block_location Where the block is located (pointer to start of data)
 */
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

/*!
 * \brief This function will update the data for the corresponding header's deallocation
 *
 * \param block_location Where the block is located (pointer to start of data)
 */
void ObjectAllocator::header_update_dealloc(GenericObject *block_location) {
  switch (config.HBlockInfo_.type_) {
    case OAConfig::hbNone: break;
    case OAConfig::hbBasic: header_basic_update_dealloc(block_location); break;
    case OAConfig::hbExtended: header_extended_update_dealloc(block_location); break;
    case OAConfig::hbExternal: header_external_update_dealloc(block_location); break;
    default: break;
  }
}

/*!
 * \brief This function will update a basic header's data for when it is deallocated
 *
 * \param block_location Where the block is located (pointer to start of data)
 */
void ObjectAllocator::header_basic_update_dealloc(GenericObject *block_location) {
  u8 *writing_location = reinterpret_cast<u8 *>(block_location) - config.PadBytes_ - config.HBlockInfo_.size_;

  u32 *allocation_number = reinterpret_cast<u32 *>(writing_location);
  (*allocation_number) = 0;

  u8 *flag = writing_location + sizeof(u32);
  int cast_flag = static_cast<int>(*flag);
  (*flag) = static_cast<u8>(cast_flag & ~1);
}

/*!
 * \brief This function will update the extended header due to deallocation
 *
 * \param block_location Where the block is located (pointer to start of data)
 */
void ObjectAllocator::header_extended_update_dealloc(GenericObject *block_location) {
  u8 *writing_location = reinterpret_cast<u8 *>(block_location) - config.PadBytes_ - config.HBlockInfo_.size_;

  writing_location += config.HBlockInfo_.additional_ + sizeof(u16);
  u32 *allocation_number = reinterpret_cast<u32 *>(writing_location);
  (*allocation_number) = 0;

  writing_location += sizeof(u32);
  u8 *flag = writing_location;
  int cast_flag = static_cast<int>(*flag);
  (*flag) = static_cast<u8>(cast_flag & ~1);
}

/*!
 * \brief This function will update the external header due to an allocation. This means it will allocate the header
 * with the corresponding data.
 *
 * \param block_location Where the block is located (pointer to start of data)
 */
void ObjectAllocator::header_external_update_dealloc(GenericObject *block_location) {
  u8 *writing_location = reinterpret_cast<u8 *>(block_location) - config.PadBytes_ - config.HBlockInfo_.size_;

  MemBlockInfo **header_ptr_ptr = reinterpret_cast<MemBlockInfo **>(writing_location);
  header_external_delete(header_ptr_ptr);
}

/*!
 * \brief This funciton will deallocate the label and external headers.
 *
 * \param header_ptr_ptr Pointer to the pointer for the header
 */
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

/*!
 * \brief Returns the header size for the given info
 *
 * \param info The structure of the header
 * \return The size of the header
 */
size_t ObjectAllocator::get_header_size(OAConfig::HeaderBlockInfo info) const {
  switch (info.type_) {
    case OAConfig::hbNone: return 0;
    case OAConfig::hbBasic: return OAConfig::BASIC_HEADER_SIZE;
    case OAConfig::hbExtended: return info.size_;
    case OAConfig::hbExternal: return sizeof(void *);
    default: return 0;
  }
}

/*!
 * \brief Returns the left alignment size
 *
 * \return The size of the left alignment bytes
 */
size_t ObjectAllocator::calculate_left_alignment_size() const {
  if (config.Alignment_ <= 0) return 0;
  size_t remainder = (sizeof(void *) + config.PadBytes_ + get_header_size(config.HBlockInfo_)) % config.Alignment_;
  return (remainder > 0) ? config.Alignment_ - remainder : 0;
}

/*!
 * \brief Returns the inter alignment size
 *
 * \return The size of the inter alignment bytes
 */
size_t ObjectAllocator::calculate_inter_alignment_size() const {
  if (config.Alignment_ <= 0) return 0;
  size_t chunk_size = get_header_size(config.HBlockInfo_) + (2 * config.PadBytes_) + object_size;

  size_t remainder = chunk_size % config.Alignment_;
  return (remainder > 0) ? config.Alignment_ - remainder : 0;
}

/*!
 * \brief Returns the size of a block with the current config
 *
 * \return The size of a block in a page
 */
size_t ObjectAllocator::calculate_block_size() const {
  return get_header_size(config.HBlockInfo_) + (2 * config.PadBytes_) + object_size + config.InterAlignSize_;
}

/*!
 * \brief Returns the size of an individual page
 *
 * \return The size of the page
 */
size_t ObjectAllocator::calculate_page_size() const {
  size_t chunk_size = get_header_size(config.HBlockInfo_) + (2 * config.PadBytes_) + object_size;

  size_t total = sizeof(void *) + config.LeftAlignSize_;
  total += config.ObjectsPerPage_ * chunk_size;
  total += (config.ObjectsPerPage_ - 1) * config.InterAlignSize_;

  return total;
}

/*!
 * \brief This function will call memset only if debug is on.
 *
 * \param object The object's block to sign
 * \param pattern The pattern to write
 * \param size The length of the signature
 */
void ObjectAllocator::write_signature(GenericObject *object, const unsigned char pattern, size_t size) {
  if (object == nullptr || !config.DebugOn_) {
    return;
  }

  memset(object, pattern, size);
}

/*!
 * \brief This function will call memset only if debug is on.
 *
 * \param location The location to sign
 * \param pattern The pattern to write
 * \param size The length of the signature
 */
void ObjectAllocator::write_signature(u8 *location, const unsigned char pattern, size_t size) {
  if (location == nullptr || !config.DebugOn_) {
    return;
  }

  memset(location, pattern, size);
}

/*!
 * \brief This will check whether the address is within the range of start and start + length
 *
 * \param start The start of the range
 * \param length The length of the range
 * \param address The location to check bounds
 *
 * \return Whether the address is inside of the provided range
 */
bool ObjectAllocator::is_in_range(u8 *start, size_t length, u8 *address) const {
  ptrdiff_t offset = address - start;
  return 0 < offset && offset < static_cast<intptr_t>(length);
}

/*!
 * \brief This function will remove a node from the given linked list
 *
 * \param head The head of the list in which the node is in.
 * \param to_remove The node in the list to remove
 */
void ObjectAllocator::generic_object_remove(GenericObject *&head, GenericObject *to_remove) {
  GenericObject *current_object = head;
  GenericObject *previous_object = nullptr;

  while (current_object != nullptr) {
    if (current_object == to_remove) {

      if (previous_object == nullptr) {
        head = current_object->Next;

      } else {
        previous_object->Next = current_object->Next;
      }

      return;
    }

    previous_object = current_object;
    current_object = current_object->Next;
  }
}
