
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <emscripten/em_asm.h>
#include <emscripten/em_js.h>
#include <exception>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

#include "CommandFile.h"
#include "talvos/EntryPoint.h"
#include "talvos/Module.h"
#include "talvos/PipelineExecutor.h"

#include <emscripten.h>
#include <emscripten/console.h>

extern "C"
{
  using PhyCoord = talvos::PipelineExecutor::PhyCoord;
  using LogCoord = talvos::PipelineExecutor::LogCoord;

  // TODO pin this representation?
  enum LaneState
  {
    Active = talvos::PipelineExecutor::LaneState::Active,
    Inactive = talvos::PipelineExecutor::LaneState::Inactive,
    AtBarrier = talvos::PipelineExecutor::LaneState::AtBarrier,
    AtBreakpoint = talvos::PipelineExecutor::LaneState::AtBreakpoint,
    AtAssert = talvos::PipelineExecutor::LaneState::AtAssert,
    AtException = talvos::PipelineExecutor::LaneState::AtException,
    NotLaunched = talvos::PipelineExecutor::LaneState::NotLaunched,
    Exited = talvos::PipelineExecutor::LaneState::Exited,
  };
  // TODO it'd be nice to check if ^ matches talvos::PipelineExecutor::LaneState

  struct ExecutionUniverse
  {
    // TODO bigger count type (requires C++ bitset-alike; i.e. roaring?)
    uint8_t Cores, Lanes;
    talvos::PipelineExecutor::StepResult Result;
    uint64_t SteppedCores, SteppedLanes;

    struct
    {
      PhyCoord PhyCoord;
      LogCoord LogCoord;
      LaneState State;
    } LaneStates[64 /* TODO */];
  };

  static_assert(sizeof(ExecutionUniverse) == 1304);
  static_assert(offsetof(ExecutionUniverse, Cores) == 0);
  static_assert(offsetof(ExecutionUniverse, Lanes) == 1);
  static_assert(offsetof(ExecutionUniverse, Result) == 2);
  static_assert(offsetof(ExecutionUniverse, SteppedCores) == 8);
  static_assert(offsetof(ExecutionUniverse, SteppedLanes) == 16);
  static_assert(offsetof(ExecutionUniverse, LaneStates) == 24);

  static_assert((sizeof(ExecutionUniverse{}.LaneStates) /
                 sizeof(ExecutionUniverse{}.LaneStates[0])) == 64);

  static_assert(sizeof(ExecutionUniverse{}.LaneStates[0]) == 20);
  static_assert(offsetof(ExecutionUniverse, LaneStates[0].PhyCoord) == 24);
  static_assert(offsetof(ExecutionUniverse, LaneStates[0].LogCoord) == 28);
  static_assert(offsetof(ExecutionUniverse, LaneStates[0].State) == 40);
  static_assert(offsetof(ExecutionUniverse, LaneStates[1]) == 44);

  static_assert(offsetof(ExecutionUniverse, LaneStates[0].PhyCoord) -
                    offsetof(ExecutionUniverse, LaneStates) ==
                0);
  static_assert(offsetof(ExecutionUniverse, LaneStates[0].LogCoord) -
                    offsetof(ExecutionUniverse, LaneStates) ==
                4);
  static_assert(offsetof(ExecutionUniverse, LaneStates[0].State) -
                    offsetof(ExecutionUniverse, LaneStates) ==
                16);

  static_assert(sizeof(PhyCoord) == 2);
  static_assert(offsetof(PhyCoord, Core) == 0);
  static_assert(offsetof(PhyCoord, Lane) == 1);

  static_assert(sizeof(LogCoord) == 12);
  static_assert(offsetof(LogCoord, X) == 0);
  static_assert(offsetof(LogCoord, Y) == 4);
  static_assert(offsetof(LogCoord, Z) == 8);

  static_assert(sizeof(LaneState) == 4);
}

class Chatty
{
public:
  Chatty() { emscripten_console_log("hello"); }
  ~Chatty() { emscripten_console_log("goodbye"); }
};

class Session
{
public:
  std::stringstream CmdStream;
  CommandFile CF;

  Session(const char *module, const char *commands)
      : CmdStream(commands), CF(module, CmdStream){};

  void run() { CF.run(); }
  ExecutionUniverse start()
  {
    CF.run(CommandFile::DEBUG);
    // run one step
    return makeU(step());
  }

  talvos::PipelineExecutor::StepResult step(uint64_t StepMask = -1)
  {
    auto res = CF.Device->getPipelineExecutor().step(StepMask);
    if (res == talvos::PipelineExecutor::FINISHED)
      // run more COMMANDs (e.g. DUMP)
      // TODO another DISPATCH won't work here (for two reasons)
      CF.run(CommandFile::DEBUG);
    return res;
  }

  talvos::PipelineExecutor::StepResult cont()
  {
    // since we don't support breakpoints yet...
    while (step() != talvos::PipelineExecutor::FINISHED)
      ;
    return talvos::PipelineExecutor::FINISHED;
  }

  void print(const std::vector<std::string> &Args)
  {
    CF.Device->getPipelineExecutor().print(Args);
  }

  void swtch(const std::vector<std::string> &Args)
  {
    CF.Device->getPipelineExecutor().swtch(Args);
  }

  void printContext()
  {
    auto &PE = CF.Device->getPipelineExecutor();
    if (PE.getCurrentInvocation())
      PE.printContext();
  }

  inline ExecutionUniverse makeU(talvos::PipelineExecutor::StepResult result,
                                 uint64_t steppedMask = -1)
  {
    ExecutionUniverse ret = {
        .Cores = static_cast<uint8_t>(CF.Device->Cores),
        .Lanes = static_cast<uint8_t>(CF.Device->Lanes),
        .Result = result,

        // TODO remove/rename this... (lolsob)
        // .SteppedCores = steppedCores,
    };

    auto getOrDef = [](const PhyCoord &k) {
      auto res = talvos::PipelineExecutor::Assignments.find(k);
      return res == talvos::PipelineExecutor::Assignments.end() ? LogCoord{}
                                                                : res->second;
    };

    int n = 0;
    for (const auto &[phyCoord, state] : talvos::PipelineExecutor::Lanes)
    {
      assert(n < (sizeof(ret.LaneStates) / sizeof(ret.LaneStates[0])));
      steppedMask &=
          ~(state != talvos::PipelineExecutor::NotLaunched ? 0x0 : (0x1 << n));
      ret.LaneStates[n++] = {.PhyCoord = phyCoord,
                             // TODO oh no, now this might not exist :(
                             .LogCoord = getOrDef(phyCoord),
                             // TODO reinterpret_cast ?
                             .State = static_cast<LaneState>(state)};
    }

    ret.SteppedLanes = steppedMask;

    assert(n == ret.Cores * ret.Lanes);
    return ret;
  }
};

// EM_JS(void, hi, (), {alert('sup')});
// --->
// extern "C"
// {
//   void hi() __attribute__((import_module("env"), import_name("hi")));
//   __attribute__((used)) static void *__em_js_ref_hi = (void *)&hi;
//   __attribute__((used))
//   __attribute__((section("em_js"), aligned(1))) char __em_js__hi[] =
//       "()"
//       "<::>"
//       "{alert('sup')}";
// };

static_assert(sizeof(talvos::Module) == 104);

// we should expect these to be Very Not Stable as the compiler/class change,
// right? But, if we can induce clang to author the JS wrapper for us,
// with these constraints in mind, will that then be stable?
//
// hm, related:
static_assert(offsetof(talvos::Module, EntryPoints) == 40);
static_assert(sizeof(talvos::Module::EntryPoints) == 12);
static_assert(sizeof(talvos::EntryPoint) == 36);
static_assert(offsetof(talvos::EntryPoint, Name) == 4);

// just some string things
static_assert(sizeof(std::string) == 12);
static_assert(sizeof(std::basic_string<char>) == sizeof(std::string));
static_assert(_LIBCPP_ABI_VERSION == 2);
#ifndef _LIBCPP_ABI_ALTERNATE_STRING_LAYOUT
#error "oh no"
#endif
static_assert(sizeof(std::string::pointer) == 4);
static_assert(sizeof(std::string::size_type) == 4);

namespace
{
using pointer = std::string::pointer;
using size_type = std::string::size_type;
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
static int __assert_bits = [] {
  // make it static so the compiler still has to write down the bit pattern.
  // I'm being a little petty.
  static __long L = {.cap = 0x0, .__is_long_ = true};
  assert(L.bits == 0x8000'0000);

  static __short S = {.size = 0x0, .__is_long_ = true};
  assert(S.bits == 0x80);

  // something something little endian so the msb of the 11th byte is
  // `__is_long_` in both cases?

  return 0;
}();

// TODO: it might be nice to pull this into its own test module
//   (w/ `-sENVIRONMENT=node`?)
static_assert(sizeof(size_t) == 4);
#ifndef _LIBCPP_ABI_ALTERNATE_STRING_LAYOUT
#error unknown string layout
#endif

#if _LIBCPP_ABI_VERSION != 2
#error unknown libcpp abi
#endif
// NB this'll be different on a 64-bit system; the wider pointers mean instead
// of a split at 11/12 characters we ought to see a split at 23/24 bytes
static int __assert_sso = [] {
  // 11 chars (10+NUL) ought to fit into the small string
  std::string str("hello worl");
  assert(str.c_str() == reinterpret_cast<char *>(&str));
  assert(str.capacity() == str.length()); // we're full up

  auto sstr = reinterpret_cast<__short *>(&str);
  assert(sstr->size == str.size());
  assert(!sstr->__is_long_);
  assert(sstr->bits == str.size());

  // but this won't fit
  std::string str2("hello world");
  assert(str2.c_str() != reinterpret_cast<char *>(&str2));
  assert(str2.capacity() > str2.length());

  auto lstr = reinterpret_cast<__long *>(&str2);
  assert(lstr->size == str2.size());
  assert(lstr->__is_long_);
  assert(lstr->bits == (0x8000'0000 | (str2.capacity() + 1)));

  return 0;
}();

} // namespace

// EM_ASM(alert('hi2'));
// ->
// ((void)emscripten_asm_const_int(
//     (__extension__({
//       __attribute__((section("em_asm"), aligned(1))) static const char x[] =
//           "alert('hi2')";
//       x;
//     })),
//     __em_asm_sig_builder<__typeof__(__em_asm_make_type_tuple())>::buffer));

// lets manually mangle some names yey
extern "C"
{
  EMSCRIPTEN_KEEPALIVE Session *Session__create__(const char *module,
                                                  const char *commands)
  {
    return new Session(module, commands);
  };
  EMSCRIPTEN_KEEPALIVE void Session__destroy__(Session *self) { delete self; };
  EMSCRIPTEN_KEEPALIVE struct talvos::Params *Session__params_ref(Session *self)
  {
    return &self->CF.Params;
  };
  EMSCRIPTEN_KEEPALIVE struct talvos::Module *Session__module_ref(Session *self)
  {
    return self->CF.Module.get();
  };

  EMSCRIPTEN_KEEPALIVE void Session_run(Session *self) { self->run(); };
  EMSCRIPTEN_KEEPALIVE void Session_start(Session *self, ExecutionUniverse *out)
  {
    assert(out);

    *out = self->start();
  };
  EMSCRIPTEN_KEEPALIVE void Session_printContext(Session *self)
  {
    self->printContext();
  };

  // interactions
  EMSCRIPTEN_KEEPALIVE talvos::PipelineExecutor::StepResult
  Session_step(Session *self, uint64_t *laneMask, ExecutionUniverse *out)
  {
    assert(out);
    assert(laneMask);

    auto ret = self->step(*laneMask);
    *out = self->makeU(ret, *laneMask);
    return ret;
  }
  EMSCRIPTEN_KEEPALIVE void Session_continue(Session *self,
                                             ExecutionUniverse *out)
  {
    *out = self->makeU(self->cont());
  }

  // inspection (NB: switch as an inspection-only concept is novel here)
  EMSCRIPTEN_KEEPALIVE void Session_print(Session *self, int argc, char *argv[])
  {
    std::vector<std::string> Args(argv, argv + argc);
    self->print(Args);
  }
  EMSCRIPTEN_KEEPALIVE void Session_switch(Session *self, int argc,
                                           char *argv[])
  {
    std::vector<std::string> Args(argv, argv + argc);
    self->swtch(Args);
  }
};

extern "C"
{
  EMSCRIPTEN_KEEPALIVE bool validate_wasm(const char *module)
  {
    std::vector<uint8_t> ModuleText((const uint8_t *)module,
                                    (const uint8_t *)module + strlen(module));

    return talvos::Module::load(ModuleText) != nullptr;
  }

  EMSCRIPTEN_KEEPALIVE Session *run_wasm(const char *module,
                                         const char *commands)
  {
    return Session__create__(module, commands);
  }

  EMSCRIPTEN_KEEPALIVE Session *debug_wasm(const char *module,
                                           const char *commands)
  {
    return Session__create__(module, commands);
  }

  EMSCRIPTEN_KEEPALIVE
  void Session_fetch_shrubbery(Session *self, uint64_t *out) { *out = -1; }

  // this kind of exception doesn't work so well in WASM-land
  // EMSCRIPTEN_KEEPALIVE void exception() { throw "hello: it's an
  // exception!";
  // }
  EMSCRIPTEN_KEEPALIVE void exception()
  {
    throw std::runtime_error("hello: it's an exception!");
  }

  EMSCRIPTEN_KEEPALIVE void assertion()
  {
    assert(false && "hello: it's an assertion!");
  }
}
