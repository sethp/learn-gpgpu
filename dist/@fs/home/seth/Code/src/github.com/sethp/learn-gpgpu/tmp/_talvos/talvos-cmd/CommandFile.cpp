// Copyright (c) 2018 the Talvos developers. All rights reserved.
//
// This file is distributed under a three-clause BSD license. For full license
// terms please see the LICENSE file distributed with this source code.

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

#include "CommandFile.h"
#include "talvos/Commands.h"
#include "talvos/ComputePipeline.h"
#include "talvos/Device.h"
#include "talvos/Dim3.h"
#include "talvos/Memory.h"
#include "talvos/Module.h"
#include "talvos/Object.h"
#include "talvos/PipelineContext.h"
#include "talvos/PipelineExecutor.h"
#include "talvos/Type.h"

using namespace std;

// Values match SPIR-V spec.
#define EXEC_MODEL_GLCOMPUTE 5

class NotRecognizedException : exception
{
};

CommandFile::CommandFile(const char *file, std::istream &CmdStream)
    : Stream(CmdStream)
{
  std::vector<uint8_t> ModuleText((const uint8_t *)file,
                                  (const uint8_t *)file + strlen(file));
  // assert(*(file + strlen(file)) == '\0');
  // assert(ModuleText.size() == strlen(file));
  Module = talvos::Module::load(ModuleText);
}

CommandFile::CommandFile(std::istream &Stream) : Stream(Stream) {}

CommandFile::~CommandFile() { delete Device; }

template <typename T> T CommandFile::get(const char *ParseAction)
{
  CurrentParseAction = ParseAction;
  while (true)
  {
    // Skip leading whitespace and check for newlines.
    while (true)
    {
      int c = Stream.peek();
      if (!isspace(c))
        break;

      if (c == '\n')
        ++CurrentLine;
      Stream.get();
    }

    // Note the current stream position.
    streampos Pos = Stream.tellg();

    // Try to read a token.
    string Token;
    Stream >> Token;
    if (Stream.fail())
      throw Stream.rdstate();

    // Check for comments.
    if (Token[0] == '#')
    {
      // Consume rest of line.
      getline(Stream, Token);
      ++CurrentLine;
      continue;
    }

    // Rewind and read token of desired type.
    T Result;
    Stream.seekg(Pos);
    Stream >> Result;
    if (Stream.fail())
      throw Stream.rdstate();

    return Result;
  }
}

void CommandFile::parseBuffer()
{
  string Name = get<string>("buffer name");
  if (Buffers.count(Name))
    throw "duplicate buffer name";

  uint64_t NumBytes = get<uint64_t>("buffer size");

  // Allocate buffer.
  uint64_t Address = Device->getGlobalMemory().allocate(NumBytes);
  Buffers[Name] = {Address, NumBytes};

  // Process initializer.
  string Init = get<string>("buffer initializer");
  if (Init == "DATA")
  {
    string InitType = get<string>("data type");
    if (InitType == "INT8")
      data<int8_t>(Address, NumBytes);
    else if (InitType == "UINT8")
      data<uint8_t>(Address, NumBytes);
    else if (InitType == "INT16")
      data<int16_t>(Address, NumBytes);
    else if (InitType == "UINT16")
      data<uint16_t>(Address, NumBytes);
    else if (InitType == "INT32")
      data<int32_t>(Address, NumBytes);
    else if (InitType == "UINT32")
      data<uint32_t>(Address, NumBytes);
    else if (InitType == "INT64")
      data<int64_t>(Address, NumBytes);
    else if (InitType == "UINT64")
      data<uint64_t>(Address, NumBytes);
    else if (InitType == "FLOAT")
      data<float>(Address, NumBytes);
    else if (InitType == "DOUBLE")
      data<double>(Address, NumBytes);
    else
      throw NotRecognizedException();
  }
  else if (Init == "FILL")
  {
    string InitType = get<string>("fill type");
    if (InitType == "INT8")
      fill<int8_t>(Address, NumBytes);
    else if (InitType == "UINT8")
      fill<uint8_t>(Address, NumBytes);
    else if (InitType == "INT16")
      fill<int16_t>(Address, NumBytes);
    else if (InitType == "UINT16")
      fill<uint16_t>(Address, NumBytes);
    else if (InitType == "INT32")
      fill<int32_t>(Address, NumBytes);
    else if (InitType == "UINT32")
      fill<uint32_t>(Address, NumBytes);
    else if (InitType == "INT64")
      fill<int64_t>(Address, NumBytes);
    else if (InitType == "UINT64")
      fill<uint64_t>(Address, NumBytes);
    else if (InitType == "FLOAT")
      fill<float>(Address, NumBytes);
    else if (InitType == "DOUBLE")
      fill<double>(Address, NumBytes);
    else
      throw NotRecognizedException();
  }
  else if (Init == "SERIES")
  {
    string InitType = get<string>("series type");
    if (InitType == "INT8")
      series<int8_t>(Address, NumBytes);
    else if (InitType == "UINT8")
      series<uint8_t>(Address, NumBytes);
    else if (InitType == "INT16")
      series<int16_t>(Address, NumBytes);
    else if (InitType == "UINT16")
      series<uint16_t>(Address, NumBytes);
    else if (InitType == "INT32")
      series<int32_t>(Address, NumBytes);
    else if (InitType == "UINT32")
      series<uint32_t>(Address, NumBytes);
    else if (InitType == "INT64")
      series<int64_t>(Address, NumBytes);
    else if (InitType == "UINT64")
      series<uint64_t>(Address, NumBytes);
    else if (InitType == "FLOAT")
      series<float>(Address, NumBytes);
    else if (InitType == "DOUBLE")
      series<double>(Address, NumBytes);
    else
      throw NotRecognizedException();
  }
  else if (Init == "BINFILE")
  {
    // Open binary data file.
    string Filename = get<string>("binary data filename");
    std::ifstream BinFile(Filename, std::ios::binary);
    if (!BinFile)
      throw "unable to open file";

    // Load data from file.
    std::vector<char> Data(NumBytes);
    if (!BinFile.read(Data.data(), NumBytes))
      throw "failed to read binary data";

    // Copy data to buffer.
    Device->getGlobalMemory().store(Address, NumBytes, (uint8_t *)Data.data());
  }
  else
  {
    throw NotRecognizedException();
  }
}

void CommandFile::parseDescriptorSet()
{
  uint32_t Set = get<uint32_t>("descriptor set");
  uint32_t Binding = get<uint32_t>("binding");
  uint32_t ArrayElement = get<uint32_t>("array element");
  string Name = get<string>("resource name");
  if (!Buffers.count(Name))
    throw "invalid resource identifier";

  DescriptorSets[Set][{Binding, ArrayElement}] = {Buffers[Name].first,
                                                  Buffers[Name].second};
}

void CommandFile::parseDispatch(Mode mode)
{
  if (!Module)
    throw "DISPATCH reached with no prior MODULE command";
  if (!Entry && !strlen(Params.EntryName))
    throw "DISPATCH reached with no prior ENTRY command";
  else if (strlen(Params.EntryName))
    if (!(Entry =
              Module->getEntryPoint(Params.EntryName, EXEC_MODEL_GLCOMPUTE)))
      throw "Bad EntryPoint!";

  talvos::Dim3 GroupCount;
  GroupCount.X = get<uint32_t>("group count X");
  GroupCount.Y = get<uint32_t>("group count Y");
  GroupCount.Z = get<uint32_t>("group count Z");

  talvos::PipelineStage *Stage =
      new talvos::PipelineStage(*Device, Module, Entry, SpecConstMap);

  CurrentPipeline.emplace(Stage);
  PC.clear();
  PC.bindComputePipeline(&*CurrentPipeline);
  PC.bindComputeDescriptors(DescriptorSets);

  CurrentDispatch = talvos::DispatchCommand(PC, {0, 0, 0}, GroupCount);
  switch (mode)
  {
  case RUN:
    CurrentDispatch->run(*Device);
    CurrentDispatch.reset();
    break;
  case DEBUG:
    Device->reportCommandBegin(&*CurrentDispatch);
    Device->getPipelineExecutor().start(*CurrentDispatch);
    break;
  default:
    throw std::runtime_error("unrecognized mode");
  }
}

void CommandFile::parseDump()
{
  string DumpType = get<string>("dump type");

  unsigned VecWidth = 1;
  size_t VPos = DumpType.find('v');
  if (VPos != string::npos)
  {
    char *End;
    VecWidth = (unsigned)strtoul(DumpType.c_str() + VPos + 1, &End, 10);
    if (VecWidth == 0 || strlen(End))
      throw "invalid vector suffix";
    DumpType = DumpType.substr(0, VPos);
  }

  if (DumpType == "INT8")
    dump<int8_t>(VecWidth);
  else if (DumpType == "UINT8")
    dump<uint8_t>(VecWidth);
  else if (DumpType == "INT16")
    dump<int16_t>(VecWidth);
  else if (DumpType == "UINT16")
    dump<uint16_t>(VecWidth);
  else if (DumpType == "INT32")
    dump<int32_t>(VecWidth);
  else if (DumpType == "UINT32")
    dump<uint32_t>(VecWidth);
  else if (DumpType == "INT64")
    dump<int64_t>(VecWidth);
  else if (DumpType == "UINT64")
    dump<uint64_t>(VecWidth);
  else if (DumpType == "FLOAT")
    dump<float>(VecWidth);
  else if (DumpType == "DOUBLE")
    dump<double>(VecWidth);
  else if (DumpType == "ALL")
    Device->getGlobalMemory().dump();
  else if (DumpType == "RAW")
  {
    string Name = get<string>("allocation name");
    if (!Buffers.count(Name))
      throw "invalid resource identifier";
    Device->getGlobalMemory().dump(Buffers.at(Name).first);
  }
  else
    throw NotRecognizedException();
}

void CommandFile::parseEndLoop()
{
  if (Loops.empty())
    throw "ENDLOOP without matching LOOP";

  if (--Loops.back().first)
    Stream.seekg(Loops.back().second);
  else
    Loops.pop_back();
}

void CommandFile::parseEntry()
{
  string Name = get<string>("entry name");
  Entry = Module->getEntryPoint(Name, EXEC_MODEL_GLCOMPUTE);
  if (!Entry)
    throw "invalid entry point";
}

void CommandFile::parseLoop()
{
  size_t Count = get<size_t>("loop count");
  if (Count == 0)
    throw "loop count must be > 0";
  Loops.push_back({Count, Stream.tellg()});
}

void CommandFile::parseModule()
{
  // Load SPIR-V module.
  string SPVFileName = get<string>("module filename");
  Module = talvos::Module::load(SPVFileName);
  if (!Module)
    throw "failed to load SPIR-V module";
}

void CommandFile::parseSpecialize()
{
  uint32_t SpecId = get<uint32_t>("spec constant ID");
  string SpecType = get<string>("specialize type");

  if (SpecType == "BOOL")
    specialize<bool>(SpecId);
  else if (SpecType == "INT16")
    specialize<int16_t>(SpecId);
  else if (SpecType == "UINT16")
    specialize<uint16_t>(SpecId);
  else if (SpecType == "INT32")
    specialize<int32_t>(SpecId);
  else if (SpecType == "UINT32")
    specialize<uint32_t>(SpecId);
  else if (SpecType == "INT64")
    specialize<int64_t>(SpecId);
  else if (SpecType == "UINT64")
    specialize<uint64_t>(SpecId);
  else if (SpecType == "FLOAT")
    specialize<float>(SpecId);
  else if (SpecType == "DOUBLE")
    specialize<double>(SpecId);
  else
    throw NotRecognizedException();
}

template <typename T> void CommandFile::dump(unsigned VecWidth)
{
  string Name = get<string>("allocation name");
  if (!Buffers.count(Name))
    throw "invalid resource identifier";

  uint64_t Address = Buffers.at(Name).first;
  uint64_t NumBytes = Buffers.at(Name).second;

  // 3-element vectors are padded to 4-elements in buffers.
  if (VecWidth == 3)
    VecWidth = 4;

  std::cout << std::endl
            << "Buffer '" << Name << "' (" << NumBytes
            << " bytes):" << std::endl;
  for (uint64_t i = 0; i < NumBytes / sizeof(T); i += VecWidth)
  {
    std::cout << "  " << Name << "[" << (i / VecWidth) << "] = ";

    if (VecWidth > 1)
      std::cout << "(";
    for (unsigned v = 0; v < VecWidth; v++)
    {
      if (v > 0)
        std::cout << ", ";

      if (i + v >= NumBytes / sizeof(T))
        break;

      T Value;
      Device->getGlobalMemory().load((uint8_t *)&Value,
                                     Address + (i + v) * sizeof(T), sizeof(T));
      std::cout << Value;
    }
    if (VecWidth > 1)
      std::cout << ")";

    std::cout << std::endl;
  }
}

template <typename T>
void CommandFile::data(uint64_t Address, uint64_t NumBytes)
{
  for (uint64_t i = 0; i < NumBytes; i += sizeof(T))
  {
    T Value = get<T>("data value");
    Device->getGlobalMemory().store(Address + i, sizeof(Value),
                                    (uint8_t *)&Value);
  }
}

template <typename T>
void CommandFile::fill(uint64_t Address, uint64_t NumBytes)
{
  T FillValue = get<T>("fill value");
  for (uint64_t i = 0; i < NumBytes; i += sizeof(FillValue))
    Device->getGlobalMemory().store(Address + i, sizeof(FillValue),
                                    (uint8_t *)&FillValue);
}

template <typename T>
void CommandFile::series(uint64_t Address, uint64_t NumBytes)
{
  T Value = get<T>("series start");
  T RangeInc = get<T>("series inc");
  for (uint64_t i = 0; i < NumBytes; i += sizeof(Value), Value += RangeInc)
    Device->getGlobalMemory().store(Address + i, sizeof(Value),
                                    (uint8_t *)&Value);
}

template <typename T> void CommandFile::specialize(uint32_t SpecId)
{
  uint32_t ResultId = Module->getSpecConstant(SpecId);
  if (!ResultId)
    throw "invalid specialization constant ID";

  const talvos::Type *Ty = Module->getObject(ResultId).getType();
  if (Ty->getSize() != sizeof(T))
    throw "wrong type size for specialization constant";

  SpecConstMap[SpecId] = talvos::Object(Ty, get<T>("specialization value"));
}

bool CommandFile::run(Mode mode)
{
  try
  {
    while (Stream.good())
    {
      // Parse command.
      string Command = get<string>("command");
      if (Command == "BUFFER")
        parseBuffer();
      else if (Command == "DESCRIPTOR_SET")
        parseDescriptorSet();
      else if (Command == "DISPATCH")
      {
        parseDispatch(mode);
        if (mode == DEBUG)
          return false;
      }
      else if (Command == "DUMP")
        parseDump();
      else if (Command == "ENDLOOP")
        parseEndLoop();
      else if (Command == "ENTRY")
        parseEntry();
      else if (Command == "LOOP")
        parseLoop();
      else if (Command == "MODULE")
        parseModule();
      else if (Command == "SPECIALIZE")
        parseSpecialize();
      else
      {
        std::cerr << "line " << CurrentLine << ": ";
        std::cerr << "Unrecognized command '" << Command << "'" << std::endl;
        return false;
      }
    }
  }
  catch (NotRecognizedException e)
  {
    std::cerr << "line " << CurrentLine << ": ";
    std::cerr << "ERROR: unrecognized " << CurrentParseAction << std::endl;
    return false;
  }
  catch (const char *err)
  {
    std::cerr << "line " << CurrentLine << ": ";
    std::cerr << "ERROR: " << err << std::endl;
    return false;
  }
  catch (ifstream::iostate e)
  {
    if (e & istream::eofbit)
    {
      // EOF is only OK if we're parsing the next command.
      if (CurrentParseAction != "command")
      {
        std::cerr << "line " << CurrentLine << ": ";
        std::cerr << "Unexpected EOF while parsing " << CurrentParseAction
                  << std::endl;
        return false;
      }
      if (!Loops.empty())
      {
        std::cerr << "line " << CurrentLine << ": ";
        std::cerr << "ERROR: Unterminated LOOP" << std::endl;
        return false;
      }
    }
    else
    {
      std::cerr << "line " << CurrentLine << ": ";
      std::cerr << "Failed to parse " << CurrentParseAction << std::endl;
      return false;
    }
  }

  return true;
}
