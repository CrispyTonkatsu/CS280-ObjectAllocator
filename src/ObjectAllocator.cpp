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

// TODO: Document all code

// TODO: Testing:
// - Memory Allocation & Deletion
// - Memory Alignment + Padding & Headers
// - Correct Debug & Stats info

// TODO: Check that there are no STL usages
#include "ObjectAllocator.h"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <ostream>

ObjectAllocator::ObjectAllocator(size_t ObjectSize, const OAConfig &config) : object_size(ObjectSize), config(config) {
  this->config.LeftAlignSize_ = static_cast<unsigned>(calculate_left_alignment_size());
  this->config.InterAlignSize_ = static_cast<unsigned>(calculate_inter_alignment_size());

  page_size = calculate_page_size();

  // TODO: Update the stats
  stats.ObjectSize_ = ObjectSize;
  stats.PageSize_ = page_size;
}

ObjectAllocator::~ObjectAllocator() {}

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

void *ObjectAllocator::allocate_page() {
  uint8_t *new_page = nullptr;
  try {
    new_page = new uint8_t[page_size];

  } catch (const std::bad_alloc &) {
    throw OAException(OAException::E_NO_MEMORY, "Bad allocation thrown by 'new' operator.");
  }

  // TODO: Write the signature well
  memset(new_page, UNALLOCATED_PATTERN, page_size);

  page_list = reinterpret_cast<GenericObject *>(new_page);
  page_list->Next = nullptr;

  return new_page;
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
  size_t remainder((object_size + (2 * config.PadBytes_) + get_header_size(config.HBlockInfo_)) % config.Alignment_);
  return (remainder > 0) ? config.Alignment_ - remainder : 0;
}

size_t ObjectAllocator::calculate_page_size() const {
  size_t total = 4 + config.LeftAlignSize_;
  total += config.ObjectsPerPage_ * (get_header_size(config.HBlockInfo_) + (2 * config.PadBytes_) + object_size);
  total += (config.ObjectsPerPage_ - 1) * config.InterAlignSize_;

  return total;
}
