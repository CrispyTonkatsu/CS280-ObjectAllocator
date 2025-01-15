/**
 * @file ObjectAllocator.cpp
 * @author Edgar Jose Donoso Mansilla (e.donosomansilla)
 * @course CS280
 * @term Spring 2025
 *
 * @brief Implementation for a basic memory manager
 */

// TODO: Testing:
// - Memory Allocation & Deletion
// - Memory Alignment + Padding & Headers
// - Correct Debug & Stats info

#include "ObjectAllocator.h"

ObjectAllocator::ObjectAllocator(size_t ObjectSize, const OAConfig &config) :
    object_size(ObjectSize), config(config), page_size(ObjectSize * config.ObjectsPerPage_) {
  stats.ObjectSize_ = object_size;
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

unsigned ObjectAllocator::DumpMemoryInUse(DUMPCALLBACK fn) const {}

unsigned ObjectAllocator::ValidatePages(VALIDATECALLBACK fn) const {}

unsigned ObjectAllocator::FreeEmptyPages() {}

bool ObjectAllocator::ImplementedExtraCredit() {
  // TODO: Implement the extra credit and change this.
  return false;
}

void ObjectAllocator::SetDebugState(bool State) { config.DebugOn_ = State; }

const void *ObjectAllocator::GetFreeList() const { return free_blocks_list; }

const void *ObjectAllocator::GetPageList() const { return page_list; }

OAConfig ObjectAllocator::GetConfig() const { return config; }

OAStats ObjectAllocator::GetStats() const { return stats; }

void *ObjectAllocator::cpp_mem_manager_allocate(const char *label) { return nullptr; }

void ObjectAllocator::cpp_mem_manager_free(void *object) {}

void *ObjectAllocator::custom_mem_manager_allocate(const char *label) { return nullptr; }

void ObjectAllocator::custom_mem_manager_free(void *object) {}
