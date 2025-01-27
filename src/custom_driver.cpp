#include "ObjectAllocator.h"

int main() {
  OAConfig config(false, 3, 10, false, 2, OAConfig::HeaderBlockInfo(OAConfig::hbExternal, 1));

  ObjectAllocator *allocator = new ObjectAllocator(16, config);

  allocator->Allocate("This is so cool");

  delete allocator;
}
