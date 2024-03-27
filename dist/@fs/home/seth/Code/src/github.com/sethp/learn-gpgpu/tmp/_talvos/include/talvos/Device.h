// Copyright (c) 2018 the Talvos developers. All rights reserved.
//
// This file is distributed under a three-clause BSD license. For full license
// terms please see the LICENSE file distributed with this source code.

/// \file Device.h
/// This file declares the Device class.

#ifndef TALVOS_DEVICE_H
#define TALVOS_DEVICE_H

#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

namespace talvos
{

class Command;
class Instruction;
class Invocation;
class Memory;
class PipelineExecutor;
class Plugin;
class Workgroup;

/// A Device instance encapsulates properties and state for the virtual device.
class Device
{
public:
  // /// The Device's number of "core"s
  const uint64_t Cores;
  // /// The Device's number of parallel SIMT lanes per "core"
  const uint64_t Lanes;

  Device(uint64_t Cores = 4, uint64_t Lanes = 8);
  ~Device();

  // Do not allow Device objects to be copied.
  ///\{
  Device(const Device &) = delete;
  Device &operator=(const Device &) = delete;
  ///\}

  /// Get the global memory instance associated with this device.
  Memory &getGlobalMemory() { return *GlobalMemory; }

  /// Returns the PipelineExecutor for this device.
  PipelineExecutor &getPipelineExecutor() { return *Executor; }

  /// Returns true if all of the loaded plugins are thread-safe.
  bool isThreadSafe() const;

  /// Notify the device that a fence was signaled.
  void notifyFenceSignaled();

  /// Report an error that has occurred during emulation.
  /// This prints \p Error to stderr along with the current execution context.
  /// If \p Fatal is true, abort() will be called after handling the error.
  void reportError(const std::string &Error, bool Fatal = false);

  /// \name Plugin notification functions.
  ///@{
  void reportAtomicAccess(const Memory *Mem, uint64_t Address,
                          uint64_t NumBytes, uint32_t Opcode, uint32_t Scope,
                          uint32_t Semantics);
  void reportCommandBegin(const Command *Cmd);
  void reportCommandComplete(const Command *Cmd);
  void reportInstructionExecuted(const Invocation *Invoc,
                                 const Instruction *Inst);
  void reportInvocationBegin(const Invocation *Invoc);
  void reportInvocationComplete(const Invocation *Invoc);
  void reportMemoryLoad(const Memory *Mem, uint64_t Address, uint64_t NumBytes);
  void reportMemoryMap(const Memory *Mem, uint64_t Base, uint64_t Offset,
                       uint64_t NumBytes);
  void reportMemoryStore(const Memory *Mem, uint64_t Address, uint64_t NumBytes,
                         const uint8_t *Data);
  void reportMemoryUnmap(const Memory *Mem, uint64_t Base);
  void reportWorkgroupBegin(const Workgroup *Group);
  void reportWorkgroupBarrier(const Workgroup *Group);
  void reportWorkgroupComplete(const Workgroup *Group);
  ///@}

  /// Wait for fences to signal.
  /// If \p WaitAll is \p true, waits for all fences to signal, otherwise waits
  /// for any single fence to signal.
  /// Returns \p true on success, or \p false if \p Timeout nanoseconds elapsed
  /// before the fences signaled.
  bool waitForFences(const std::vector<const bool *> &Fences, bool WaitAll,
                     uint64_t Timeout) const;

private:
  Memory *GlobalMemory; ///< The global memory of this device.

  /// List of plugins that are currently loaded.
  std::vector<std::pair<void *, Plugin *>> Plugins;

  /// The pipeline executor instance.
  PipelineExecutor *Executor;

  /// The maximum number of errors to report.
  size_t MaxErrors;

  /// A mutex for synchronizing threads waiting on fence signals.
  mutable std::mutex FenceMutex;

  /// Condition variable to notify threads waiting on fence signals.
  mutable std::condition_variable FenceSignaled;
};

} // namespace talvos

#endif
