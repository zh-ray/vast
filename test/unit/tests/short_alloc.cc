#include "framework/unit.h"

#include <forward_list>
#include "vast/util/alloc.h"

SUITE("util")

using namespace vast;

TEST("stack allocator")
{
  using allocator = util::short_alloc<uint64_t, 16>;
  using short_list = std::forward_list<uint64_t, allocator>;

  util::arena<16> arena;
  allocator alloc{arena};
  short_list list{alloc};

  // Use the stack.
  list.push_front(21);

  // Arena is now full.
  CHECK(arena.used() == arena.size());

  // Use the heap.
  list.push_front(42);
  list.push_front(84);
  list.push_front(168);
}