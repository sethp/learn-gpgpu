// Copyright (c) 2018 the Talvos developers. All rights reserved.
//
// This file is distributed under a three-clause BSD license. For full license
// terms please see the LICENSE file distributed with this source code.

#include <fstream>
#include <map>
#include <memory>
#include <optional>
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <utility>
#include <vector>

#include "talvos/Commands.h"
#include "talvos/ComputePipeline.h"
#include "talvos/Device.h"
#include "talvos/PipelineContext.h"
#include "talvos/PipelineStage.h"

namespace talvos
{
class EntryPoint;
class Module;
extern "C" struct Params
{
  // used to populate Entry at dispatch time, if set
  const char EntryName[64] = {'\0'};
};
} // namespace talvos

class CommandFile
{
public:
  enum Mode
  {
    RUN,
    DEBUG
  };

  CommandFile(const char *module, std::istream &CmdStream);
  CommandFile(std::istream &Stream);
  ~CommandFile();
  bool run(Mode mode = RUN);

  talvos::Device *Device = new talvos::Device();

private:
  template <typename T> T get(const char *ParseAction);

  void parseBuffer();
  void parseDescriptorSet();
  void parseDispatch(Mode);
  void parseDump();
  void parseEndLoop();
  void parseEntry();
  void parseLoop();
  void parseModule();
  void parseSpecialize();

  template <typename T> void dump(unsigned VecWidth);
  template <typename T> void data(uint64_t Address, uint64_t NumBytes);
  template <typename T> void fill(uint64_t Address, uint64_t NumBytes);
  template <typename T> void series(uint64_t Address, uint64_t NumBytes);
  template <typename T> void specialize(uint32_t SpecId);

  std::istream &Stream;
  bool Interactive = false;
  std::map<std::string, std::pair<uint64_t, uint64_t>> Buffers;
  talvos::SpecConstantMap SpecConstMap;
  talvos::DescriptorSetMap DescriptorSets;
  std::vector<std::pair<size_t, std::streampos>> Loops;

  size_t CurrentLine = 1;
  std::string CurrentParseAction;

public:
  const talvos::EntryPoint *Entry = nullptr;
  std::shared_ptr<talvos::Module> Module;
  std::optional<talvos::DispatchCommand> CurrentDispatch;
  std::optional<talvos::ComputePipeline> CurrentPipeline;
  talvos::PipelineContext PC;

  struct talvos::Params Params;
};
