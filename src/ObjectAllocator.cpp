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

// TODO: Make sure that all exception messages match what is in the handout requirements

// TODO: Check that there are no STL usages
#include "ObjectAllocator.h"
#include <cstdint>
#include <cstring>

using u8 = uint8_t;

ObjectAllocator::ObjectAllocator(size_t ObjectSize, const OAConfig &config) :
    object_size(ObjectSize), config(config), block_size(calculate_block_size()) {
  this->config.LeftAlignSize_ = static_cast<unsigned>(calculate_left_alignment_size());
  this->config.InterAlignSize_ = static_cast<unsigned>(calculate_inter_alignment_size());

  page_size = calculate_page_size();

  // TODO: Update the stats
  stats.ObjectSize_ = ObjectSize;
  stats.PageSize_ = page_size;

  GenericObject *new_page = allocate_page();
  bind_page(new_page);
}

ObjectAllocator::~ObjectAllocator() {
  // TODO: Delete make sure to cleanup properly later
  delete[] reinterpret_cast<u8 *>(page_list);
}

void *ObjectAllocator::Allocate(const char *label) {
  // TODO: Add debug checks
  // TODO: Update stats
  stats.Allocations_++;

  if (config.UseCPPMemManager_) {
    return cpp_mem_manager_allocate(label);
  }
  return custom_mem_manager_allocate(label);
}

void ObjectAllocator::Free(void *Object) {
  // TODO: Add debug checks
  // TODO: Update Stats
  stats.Deallocations_++;

  if (config.UseCPPMemManager_) {
    cpp_mem_manager_free(Object);
    return;
  }
  custom_mem_manager_free(Object);
}

unsigned ObjectAllocator::DumpMemoryInUse(DUMPCALLBACK) const { return 0; }

unsigned ObjectAllocator::ValidatePages(VALIDATECALLBACK) const { return 0; }

unsigned ObjectAllocator::FreeEmptyPages() { return 0; }

bool ObjectAllocator::ImplementedExtraCredit() {
  // TODO: Implement the extra credit and change this.
  return false;
}

void ObjectAllocator::SetDebugState(bool State) { config.DebugOn_ = State; }

const void *ObjectAllocator::GetFreeList() const { return free_blocks_list; }

const void *ObjectAllocator::GetPageList() const { return page_list; }

OAConfig ObjectAllocator::GetConfig() const { return config; }

OAStats ObjectAllocator::GetStats() const { return stats; }

void *ObjectAllocator::cpp_mem_manager_allocate(const char *) { return nullptr; }

void ObjectAllocator::cpp_mem_manager_free(void *) {}

void *ObjectAllocator::custom_mem_manager_allocate(const char *) { return nullptr; }

void ObjectAllocator::custom_mem_manager_free(void *) {}

GenericObject *ObjectAllocator::allocate_page() {
  if (stats.PagesInUse_ + 1 > config.MaxPages_) {
    throw OAException(OAException::E_NO_PAGES, "The maximum amount of pages has been allocated");
  }

  u8 *new_page = nullptr;
  try {
    new_page = new u8[page_size];

  } catch (const std::bad_alloc &) {
    throw OAException(OAException::E_NO_MEMORY, "Bad allocation thrown by 'new' operator.");
  }

  if (config.DebugOn_) {
    // TODO: Implement a signing function (debug pattern)
  }

  GenericObject *new_obj = reinterpret_cast<GenericObject *>(new_page);
  new_obj->Next = nullptr;

  return new_obj;
}

void ObjectAllocator::bind_page(GenericObject *page) {
  if (page == nullptr) {
    return;
  }

  // TODO: Add signing here if that is needed

  u8 *raw_page = reinterpret_cast<u8 *>(page);

  u8 *previous_block = nullptr;
  u8 *current_block = raw_page + sizeof(void *) + config.LeftAlignSize_ + config.HBlockInfo_.size_ + config.PadBytes_;

  // TODO: Add padding bytes as you go (probably abstract this into a separate function)
  for (size_t i = 0; i < config.ObjectsPerPage_; i++) {
    GenericObject *previous_object = reinterpret_cast<GenericObject *>(previous_block);
    GenericObject *current_object = reinterpret_cast<GenericObject *>(current_block);

    current_object->Next = previous_object;

    previous_block = current_block;
    current_block += block_size;
  }

  free_blocks_list = reinterpret_cast<GenericObject *>(current_block);

  page->Next = page_list;
  page_list = page;
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
  size_t remainder = ((sizeof(void *) + config.PadBytes_ + get_header_size(config.HBlockInfo_)) % config.Alignment_);
  return (remainder > 0) ? config.Alignment_ - remainder : 0;
}

size_t ObjectAllocator::calculate_inter_alignment_size() const {
  if (config.Alignment_ <= 0) return 0;
  size_t remainder(block_size % config.Alignment_);
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
