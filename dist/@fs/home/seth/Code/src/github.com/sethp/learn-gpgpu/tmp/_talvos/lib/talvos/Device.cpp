// Copyright (c) 2018 the Talvos developers. All rights reserved.
//
// This file is distributed under a three-clause BSD license. For full license
// terms please see the LICENSE file distributed with this source code.

/// \file Device.cpp
/// This file defines the Device class.

#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable> // for condition_variable
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <mutex> // for mutex, unique_lock, lock_guard
#include <sstream>
#include <stdint.h> // for uint64_t, uint32_t, uint8_t
#include <string>   // for operator<<, allocator, getline
#include <utility>  // for pair
#include <vector>   // for vector

#if defined(_WIN32) && !defined(__MINGW32__)
#include <windows.h>
#undef ERROR
#undef VOID
#else
#include <dlfcn.h>
#endif

#include "Utils.h"
#include "talvos/Device.h"
#include "talvos/Dim3.h"
#include "talvos/EntryPoint.h"
#include "talvos/Instruction.h"
#include "talvos/Invocation.h"
#include "talvos/Memory.h"
#include "talvos/PipelineExecutor.h"
#include "talvos/PipelineStage.h"
#include "talvos/Plugin.h"
#include "talvos/Workgroup.h"

namespace talvos
{
class Command;
}

namespace talvos
{

typedef Plugin *(*CreatePluginFunc)(const Device *);
typedef void (*DestroyPluginFunc)(Plugin *);

// Counter for the number of errors reported.
static std::atomic<size_t> NumErrors;

Device::Device(uint64_t Cores, uint64_t Lanes) : Cores(Cores), Lanes(Lanes)
{
  GlobalMemory = new Memory(*this, MemoryScope::Device);

  // Load plugins from dynamic libraries.
  const char *PluginList = getenv("TALVOS_PLUGINS");
  if (PluginList)
  {
    std::istringstream SS(PluginList);
    std::string LibPath;
    while (std::getline(SS, LibPath, ';'))
    {
#if defined(_WIN32) && !defined(__MINGW32__)
      // Open plugin library file.
      HMODULE Library = LoadLibraryA(LibPath.c_str());
      if (!Library)
      {
        std::cerr << "Talvos: Failed to load plugin '" << LibPath << "' - "
                  << GetLastError() << std::endl;
        continue;
      }

      // Get handle to plugin creation function.
      void *Create = GetProcAddress(Library, "talvosCreatePlugin");
      if (!Create)
      {
        std::cerr << "Talvos: Failed to load plugin '" << LibPath << "' - "
                  << GetLastError() << std::endl;
        continue;
      }
#else
      // Open plugin library file.
      void *Library = dlopen(LibPath.c_str(), RTLD_NOW);
      if (!Library)
      {
        std::cerr << "Talvos: Failed to load plugin '" << LibPath << "' - "
                  << dlerror() << std::endl;
        continue;
      }

      // Get handle to plugin creation function.
      void *Create = dlsym(Library, "talvosCreatePlugin");
      if (!Create)
      {
        std::cerr << "Talvos: Failed to load plugin '" << LibPath << "' - "
                  << dlerror() << std::endl;
        continue;
      }
#endif

      // Create plugin and add to list.
      Plugin *P = ((CreatePluginFunc)Create)(this);
      Plugins.push_back({Library, P});
    }
  }

  Executor = new PipelineExecutor(PipelineExecutorKey(), *this);

  NumErrors = 0;
  MaxErrors = getEnvUInt("TALVOS_MAX_ERRORS", 100);
}

Device::~Device()
{
  // Destroy plugins and unload their dynamic libraries.
  for (auto P : Plugins)
  {
#if defined(_WIN32) && !defined(__MINGW32__)
    void *Destroy = GetProcAddress((HMODULE)P.first, "talvosDestroyPlugin");
    if (Destroy)
      ((DestroyPluginFunc)Destroy)(P.second);
    FreeLibrary((HMODULE)P.first);
#else
    void *Destroy = dlsym(P.first, "talvosDestroyPlugin");
    if (Destroy)
      ((DestroyPluginFunc)Destroy)(P.second);
    dlclose(P.first);
#endif
  }

  delete Executor;
  delete GlobalMemory;
}

bool Device::isThreadSafe() const
{
  for (auto P : Plugins)
    if (!P.second->isThreadSafe())
      return false;
  return true;
}

void Device::notifyFenceSignaled()
{
  std::unique_lock<std::mutex> Lock(FenceMutex);
  FenceSignaled.notify_all();
}

void Device::reportError(const std::string &Error, bool Fatal)
{
  // Increment error count, exit early if we have hit the limit.
  size_t ErrorIdx = NumErrors++;
  if (ErrorIdx >= MaxErrors)
    return;

  // Guard output to avoid mangling error messages from multiple threads.
  static std::mutex ErrorMutex;
  std::lock_guard<std::mutex> Lock(ErrorMutex);

  std::cerr << std::endl;
  std::cerr << Error << std::endl;

  if (Executor->isWorkerThread())
  {
    // Show current entry point.
    const PipelineStage &Stage = Executor->getCurrentStage();
    const EntryPoint *EP = Stage.getEntryPoint();
    std::cerr << "    Entry point:";
    std::cerr << " %" << EP->getId();
    std::cerr << " " << EP->getName();
    std::cerr << std::endl;

    // Show current invocation and group.
    const Invocation *Inv = Executor->getCurrentInvocation();
    const Workgroup *Group = Executor->getCurrentWorkgroup();
    if (Inv)
    {
      std::cerr << "    Invocation:";
      std::cerr << " Global" << Inv->getGlobalId();
      if (Group)
      {
        std::cerr << " Local" << Inv->getGlobalId() % Stage.getGroupSize();
        std::cerr << " Group" << Group->getGroupId();
      }
    }
    else if (Group)
    {
      std::cerr << "    Group: " << Group->getGroupId();
    }
    else
    {
      std::cerr << "    <entity unknown>";
    }
    std::cerr << std::endl;

    if (Inv)
    {
      // Show current instruction.
      std::cerr << "      ";
      Inv->getCurrentInstruction()->print(std::cerr, false);
    }
    else
    {
      std::cerr << "    <location unknown>";
    }

    std::cerr << std::endl;
  }
  else
  {
    std::cerr << "    <origin unknown>" << std::endl;
  }

  std::cerr << std::endl;

  // Display warning if maximum number of errors reached.
  if (ErrorIdx == MaxErrors - 1)
  {
    std::cerr << "WARNING: " << MaxErrors << " errors reported - "
              << "suppressing further errors" << std::endl;
    std::cerr << "(configure this limit with TALVOS_MAX_ERRORS)" << std::endl;
    std::cerr << std::endl;
  }

  Executor->signalError();

  if (Fatal)
    abort();
}

#define REPORT(func, ...)                                                      \
  for (auto &P : Plugins)                                                      \
  {                                                                            \
    P.second->func(__VA_ARGS__);                                               \
  }

void Device::reportAtomicAccess(const Memory *Mem, uint64_t Address,
                                uint64_t NumBytes, uint32_t Opcode,
                                uint32_t Scope, uint32_t Semantics)
{
  const Invocation *Invoc = Executor->getCurrentInvocation();
  assert(Invoc);
  REPORT(atomicAccess, Mem, Address, NumBytes, Opcode, Scope, Semantics, Invoc);
}

void Device::reportCommandBegin(const Command *Cmd)
{
  REPORT(commandBegin, Cmd);
}

void Device::reportCommandComplete(const Command *Cmd)
{
  REPORT(commandComplete, Cmd);
}

void Device::reportInstructionExecuted(const Invocation *Invoc,
                                       const Instruction *Inst)
{
  REPORT(instructionExecuted, Invoc, Inst);
}

void Device::reportInvocationBegin(const Invocation *Invoc)
{
  REPORT(invocationBegin, Invoc);
}

void Device::reportInvocationComplete(const Invocation *Invoc)
{
  REPORT(invocationComplete, Invoc);
}

void Device::reportMemoryLoad(const Memory *Mem, uint64_t Address,
                              uint64_t NumBytes)
{
  if (Executor->isWorkerThread())
  {
    // TODO: Workgroup/subgroup level accesses?
    // TODO: Workgroup/Invocation scope initialization is not covered.
    if (auto *I = Executor->getCurrentInvocation())
      REPORT(memoryLoad, Mem, Address, NumBytes, I);
  }
  else if (Mem->getScope() == MemoryScope::Device)
  {
    REPORT(hostMemoryLoad, Mem, Address, NumBytes);
  }
}

void Device::reportMemoryMap(const Memory *Memory, uint64_t Base,
                             uint64_t Offset, uint64_t NumBytes)
{
  REPORT(memoryMap, Memory, Base, Offset, NumBytes);
}

void Device::reportMemoryStore(const Memory *Mem, uint64_t Address,
                               uint64_t NumBytes, const uint8_t *Data)
{
  if (Executor->isWorkerThread())
  {
    // TODO: Workgroup/subgroup level accesses?
    // TODO: Workgroup/Invocation scope initialization is not covered.
    if (auto *I = Executor->getCurrentInvocation())
      REPORT(memoryStore, Mem, Address, NumBytes, Data, I);
  }
  else if (Mem->getScope() == MemoryScope::Device)
  {
    REPORT(hostMemoryStore, Mem, Address, NumBytes, Data);
  }
}

void Device::reportMemoryUnmap(const Memory *Memory, uint64_t Base)
{
  REPORT(memoryUnmap, Memory, Base);
}

void Device::reportWorkgroupBegin(const Workgroup *Group)
{
  REPORT(workgroupBegin, Group);
}

void Device::reportWorkgroupBarrier(const Workgroup *Group)
{
  REPORT(workgroupBarrier, Group);
}

void Device::reportWorkgroupComplete(const Workgroup *Group)
{
  REPORT(workgroupComplete, Group);
}

#undef REPORT

bool Device::waitForFences(const std::vector<const bool *> &Fences,
                           bool WaitAll, uint64_t Timeout) const
{
  // Loop until fences have signaled or the timeout is exceeded.
  auto Start = std::chrono::system_clock::now();
  while (true)
  {
    // Lock so that we do not miss a fence signal notification.
    std::unique_lock<std::mutex> Lock(FenceMutex);

    // Check status of fences.
    uint32_t SignalCount = 0;
    for (const auto Fence : Fences)
    {
      if (*Fence)
        SignalCount++;
    }
    if (WaitAll ? SignalCount == (uint32_t)(Fences.size()) : SignalCount > 0)
      return true;

    // Check if timeout has been exceeded.
    auto Duration = std::chrono::system_clock::now() - Start;
    auto DurationNanoseconds =
        std::chrono::duration_cast<std::chrono::nanoseconds>(Duration);
    uint64_t ElapsedTime = DurationNanoseconds.count();
    if (ElapsedTime >= Timeout)
      return false;

    // Wait for another fence to signal.
    FenceSignaled.wait_for(
        Lock, std::chrono::nanoseconds((uint64_t)(Timeout - ElapsedTime)));
  }
}

} // namespace talvos
