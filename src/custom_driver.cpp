#include "ObjectAllocator.h"

int main() {
  OAConfig config(false, 3, 10, false, 2, OAConfig::HeaderBlockInfo(OAConfig::hbBasic), 16);

  ObjectAllocator *allocator = new ObjectAllocator(16, config);

  delete allocator;
}
