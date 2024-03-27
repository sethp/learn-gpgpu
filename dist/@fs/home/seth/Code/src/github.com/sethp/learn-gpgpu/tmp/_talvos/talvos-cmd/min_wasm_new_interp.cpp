
#include <string>

#if 0
// #include <cassert>
// #include <cstddef>
// #include <cstdint>
// #include <cstdlib>
// #include <cstring>
#include <climits>
#include <type_traits>

// #include <vector>

// #include "CommandFile.h"
// #include "talvos/EntryPoint.h"
// #include "talvos/Module.h"
// #include "talvos/PipelineExecutor.h"

// #include <emscripten.h>
// #include <emscripten/console.h>

extern "C"
{
  //   using PhyCoord = talvos::PipelineExecutor::PhyCoord;
  //   using LogCoord = talvos::PipelineExecutor::LogCoord;

  //   // TODO pin this representation?
  //   enum LaneState
  //   {
  //     Active = talvos::PipelineExecutor::LaneState::Active,
  //     Inactive = talvos::PipelineExecutor::LaneState::Inactive,
  //     AtBarrier = talvos::PipelineExecutor::LaneState::AtBarrier,
  //     AtBreakpoint = talvos::PipelineExecutor::LaneState::AtBreakpoint,
  //     AtAssert = talvos::PipelineExecutor::LaneState::AtAssert,
  //     AtException = talvos::PipelineExecutor::LaneState::AtException,
  //     NotLaunched = talvos::PipelineExecutor::LaneState::NotLaunched,
  //     Exited = talvos::PipelineExecutor::LaneState::Exited,
  //   };
  //   // TODO it'd be nice to check if ^ matches
  //   talvos::PipelineExecutor::LaneState

  //   struct ExecutionUniverse
  //   {
  //     // TODO bigger count type (requires C++ bitset-alike; i.e. roaring?)
  //     uint8_t Cores, Lanes;
  //     talvos::PipelineExecutor::StepResult Result;
  //     uint64_t SteppedCores, SteppedLanes;

  //     struct
  //     {
  //       PhyCoord PhyCoord;
  //       LogCoord LogCoord;
  //       LaneState State;
  //     } LaneStates[64 /* TODO */];
  //   };

  //   static_assert(sizeof(ExecutionUniverse) == 1304);
  //   static_assert(offsetof(ExecutionUniverse, Cores) == 0);
  //   static_assert(offsetof(ExecutionUniverse, Lanes) == 1);
  //   static_assert(offsetof(ExecutionUniverse, Result) == 2);
  //   static_assert(offsetof(ExecutionUniverse, SteppedCores) == 8);
  //   static_assert(offsetof(ExecutionUniverse, SteppedLanes) == 16);
  //   static_assert(offsetof(ExecutionUniverse, LaneStates) == 24);

  //   static_assert((sizeof(ExecutionUniverse{}.LaneStates) /
  //                  sizeof(ExecutionUniverse{}.LaneStates[0])) == 64);

  //   static_assert(sizeof(ExecutionUniverse{}.LaneStates[0]) == 20);
  //   static_assert(offsetof(ExecutionUniverse, LaneStates[0].PhyCoord) == 24);
  //   static_assert(offsetof(ExecutionUniverse, LaneStates[0].LogCoord) == 28);
  //   static_assert(offsetof(ExecutionUniverse, LaneStates[0].State) == 40);
  //   static_assert(offsetof(ExecutionUniverse, LaneStates[1]) == 44);

  //   static_assert(offsetof(ExecutionUniverse, LaneStates[0].PhyCoord) -
  //                     offsetof(ExecutionUniverse, LaneStates) ==
  //                 0);
  //   static_assert(offsetof(ExecutionUniverse, LaneStates[0].LogCoord) -
  //                     offsetof(ExecutionUniverse, LaneStates) ==
  //                 4);
  //   static_assert(offsetof(ExecutionUniverse, LaneStates[0].State) -
  //                     offsetof(ExecutionUniverse, LaneStates) ==
  //                 16);

  //   static_assert(sizeof(PhyCoord) == 2);
  //   static_assert(offsetof(PhyCoord, Core) == 0);
  //   static_assert(offsetof(PhyCoord, Lane) == 1);

  //   static_assert(sizeof(LogCoord) == 12);
  //   static_assert(offsetof(LogCoord, X) == 0);
  //   static_assert(offsetof(LogCoord, Y) == 4);
  //   static_assert(offsetof(LogCoord, Z) == 8);

  //   static_assert(sizeof(LaneState) == 4);
}

// static_assert(sizeof(talvos::Module) == 104);

// // we should expect these to be Very Not Stable as the compiler/class change,
// // right? But, if we can induce clang to author the JS wrapper for us,
// // with these constraints in mind, will that then be stable?
// //
// // hm, related:
// static_assert(offsetof(talvos::Module, EntryPoints) == 40);
// static_assert(sizeof(talvos::Module::EntryPoints) == 12);
// static_assert(sizeof(talvos::EntryPoint) == 36);
// static_assert(offsetof(talvos::EntryPoint, Name) == 4);

// // just some string things
// static_assert(sizeof(std::string) == 12);
// static_assert(sizeof(std::basic_string<char>) == sizeof(std::string));
// static_assert(_LIBCPP_ABI_VERSION == 2);
// #ifndef _LIBCPP_ABI_ALTERNATE_STRING_LAYOUT
// #error "oh no"
// #endif
// static_assert(sizeof(std::string::pointer) == 4);
// static_assert(sizeof(std::string::size_type) == 4);

namespace
{
// using pointer = std::string::pointer;
// using size_type = std::string::size_type;
using pointer = char *;
using size_type = unsigned long;
using value_type = char;

struct __long
{
  pointer data;
  size_type size;
  union
  {
    struct
    {
      size_type cap : sizeof(size_type) * CHAR_BIT - 1;
      size_type __is_long_ : 1;
    };
    size_type bits;
  };
};

enum
{
  __min_cap = (sizeof(__long) - 1) / sizeof(value_type) > 2
                  ? (sizeof(__long) - 1) / sizeof(value_type)
                  : 2
};

struct __short
{
  value_type data[__min_cap];
  unsigned char __padding_[sizeof(value_type) - 1];
  union
  {
    struct
    {
      unsigned char size : 7, __is_long_ : 1;
    };
    unsigned char bits;
  };
};

static_assert(std::has_unique_object_representations_v<__long>);
static_assert(offsetof(__long, data) == 0);
static_assert(offsetof(__long, size) == 4);
static_assert(offsetof(__long, bits) == 8);

static_assert(std::has_unique_object_representations_v<__short>);
static_assert(__min_cap == 11);
static_assert(offsetof(__short, data) == 0);
static_assert(offsetof(__short, bits) == 11);

// https://stackoverflow.com/questions/76932233/trying-to-use-stdbit-cast-with-a-bitfield-struct-why-is-it-not-constexpr
// well, looks like compiler support hasn't caught up yet to c++20
// static_assert([] {
//   struct __short
//   {
//     value_type data[__min_cap];
//     unsigned char __padding_[sizeof(value_type) - 1];
//     struct
//     {
//       unsigned char size : 7, __is_long_ : 1;
//     } bits;
//   } S = {.bits = {.size = 0x0, .__is_long_ = true}};

//   return std::bit_cast<unsigned char>(S.bits);
// }() == 0x80);

// can't do this with static_assert (i.e. constexpr), because we're switching
// active members. yes, it's possible that the thing wasn't fully initialized,
// but :shrug:
// static int __assert_bits = [] {
//   // make it static so the compiler still has to write down the bit pattern.
//   // I'm being a little petty.
//   static __long L = {.cap = 0x0, .__is_long_ = true};
//   assert(L.bits == 0x8000'0000);

//   static __short S = {.size = 0x0, .__is_long_ = true};
//   assert(S.bits == 0x80);

//   // something something little endian so the msb of the 11th byte is
//   // `__is_long_` in both cases?

//   return 0;
// }();

// NB this'll be different on a 64-bit system; the wider pointers mean instead
// of a split at 11/12 characters we ought to see a split at 23/24 bytes
static_assert(sizeof(size_t) == 4);
#ifndef _LIBCPP_ABI_ALTERNATE_STRING_LAYOUT
#error unknown string layout
#endif

#if _LIBCPP_ABI_VERSION != 2
#error unknown libcpp abi
#endif
// static_assert(STRING_ABI)
// static int __assert_sso = [] {
//   // 11 chars (10+NUL) ought to fit into the small string
//   std::string str("hello worl");
//   assert(str.c_str() == reinterpret_cast<char *>(&str));
//   assert(str.capacity() == str.length()); // we're full up

//   auto sstr = reinterpret_cast<__short *>(&str);
//   assert(sstr->size == str.size());
//   assert(!sstr->__is_long_);
//   assert(sstr->bits == 0x80);

//   // but this won't
//   std::string str2("hello world");
//   assert(str2.c_str() != reinterpret_cast<char *>(&str2));
//   assert(str.capacity() >= str.length());

//   auto lstr = reinterpret_cast<__long *>(&str2);
//   assert(lstr->size == str2.size());
//   assert(lstr->__is_long_);
//   assert(lstr->bits == 0x8000'0000);

//   return 0;
// }();

} // namespace
#endif
