#include "ObjectAllocator.h"

int main() {
  ObjectAllocator *test_allocator = new ObjectAllocator(16, OAConfig());

  delete test_allocator;
}
