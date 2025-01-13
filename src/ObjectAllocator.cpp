/**
 * @file ObjectAllocator.cpp
 * @author Edgar Jose Donoso Mansilla (e.donosomansilla)
 * @course CS280
 * @term Spring 2025
 *
 * @brief Implementation for a basic memory manager
 */

#include "ObjectAllocator.h"

ObjectAllocator::ObjectAllocator(size_t ObjectSize, const OAConfig &config) {}

ObjectAllocator::~ObjectAllocator() {}

void *ObjectAllocator::Allocate(const char *label) { return nullptr; }

void ObjectAllocator::Free(void *Object) {}

unsigned ObjectAllocator::DumpMemoryInUse(DUMPCALLBACK fn) const {}

unsigned ObjectAllocator::ValidatePages(VALIDATECALLBACK fn) const {}

unsigned ObjectAllocator::FreeEmptyPages() {}

bool ObjectAllocator::ImplementedExtraCredit() {
  // TODO: Implement the extra credit and change this.
  return false;
}

void ObjectAllocator::SetDebugState(bool State) {}

const void *ObjectAllocator::GetFreeList() const {}

const void *ObjectAllocator::GetPageList() const {}

OAConfig ObjectAllocator::GetConfig() const {}

OAStats ObjectAllocator::GetStats() const {}
