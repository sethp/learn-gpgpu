// Copyright (c) 2018 the Talvos developers. All rights reserved.
//
// This file is distributed under a three-clause BSD license. For full license
// terms please see the LICENSE file distributed with this source code.

/// \file Module.cpp
/// This file defines the Module class.

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <spirv-tools/libspirv.h>
#include <spirv-tools/libspirv.hpp>

#include <spirv/unified1/spirv.h>

#include "talvos/Block.h"
#include "talvos/EntryPoint.h"
#include "talvos/Function.h"
#include "talvos/Instruction.h"
#include "talvos/Module.h"
#include "talvos/Type.h"
#include "talvos/Variable.h"
#include "talvos/gdb.h"

namespace talvos
{

/// Internal class used to construct a Module during SPIRV-Tools parsing.
class ModuleBuilder
{
public:
  /// Initialize the module builder.
  void init(uint32_t IdBound)
  {
    assert(!Mod && "Module already initialized");
    Mod = std::unique_ptr<Module>(new Module(IdBound));
    CurrentFunction = nullptr;
    CurrentBlock = nullptr;
    PreviousInstruction = nullptr;
  }

  /// Process a parsed SPIR-V instruction.
  void processInstruction(const spv_parsed_instruction_t *Inst)
  {
    assert(Mod);

    if (Inst->opcode == SpvOpFunction)
    {
      assert(CurrentFunction == nullptr);
      const Type *FuncType =
          Mod->getType(Inst->words[Inst->operands[3].offset]);
      CurrentFunction = std::make_unique<Function>(Inst->result_id, FuncType);

      // Check if this is an entry point.
      if (EntryPoints.count(Inst->result_id))
      {
        EntryPointSpec EPS = EntryPoints.at(Inst->result_id);

        // Gather the list of input/output variables used by the entry point.
        VariableList Variables;
        for (auto V : EPS.Variables)
        {
          auto ModVars = Mod->getVariables();
          auto VarItr =
              std::find_if(ModVars.begin(), ModVars.end(),
                           [V](auto Var) { return Var->getId() == V; });
          assert(VarItr != ModVars.end());
          Variables.push_back(*VarItr);
        }

        // Create the entry point and add it to the module.
        Mod->addEntryPoint(new EntryPoint(Inst->result_id, EPS.Name,
                                          EPS.ExecutionModel,
                                          CurrentFunction.get(), Variables));
      }
    }
    else if (Inst->opcode == SpvOpFunctionEnd)
    {
      assert(CurrentFunction);
      assert(CurrentBlock);
      CurrentFunction->addBlock(std::move(CurrentBlock));
      Mod->addFunction(std::move(CurrentFunction));
      CurrentFunction = nullptr;
      CurrentBlock = nullptr;
    }
    else if (Inst->opcode == SpvOpFunctionParameter)
    {
      CurrentFunction->addParam(Inst->result_id);
    }
    else if (Inst->opcode == SpvOpLabel)
    {
      if (CurrentBlock)
        // Add previous block to function.
        CurrentFunction->addBlock(std::move(CurrentBlock));
      else
        // First block - set as entry block.
        CurrentFunction->setFirstBlock(Inst->result_id);

      // Create new block.
      CurrentBlock = std::make_unique<Block>(Inst->result_id);
      PreviousInstruction = &CurrentBlock->getLabel();
    }
    else if (CurrentFunction)
    {
      // Skip OpLine/OpNoLine instructions.
      if (Inst->opcode == SpvOpLine || Inst->opcode == SpvOpNoLine)
        return;

      // Create an array of operand values.
      uint32_t *Operands = new uint32_t[Inst->num_operands];
      for (int i = 0; i < Inst->num_operands; i++)
      {
        // TODO: Handle larger operands
        assert(Inst->operands[i].num_words == 1);
        Operands[i] = Inst->words[Inst->operands[i].offset];
      }

      // Create the instruction.
      const Type *ResultType =
          Inst->type_id ? Mod->getType(Inst->type_id) : nullptr;
      Instruction *I = new Instruction(Inst->opcode, Inst->num_operands,
                                       Operands, ResultType);
      delete[] Operands;

      // Insert this instruction into the current block.
      assert(PreviousInstruction);
      I->insertAfter(PreviousInstruction);
      PreviousInstruction = I;
    }
    else
    {
      switch (Inst->opcode)
      {
      case SpvOpCapability:
      {
        uint32_t Capability = Inst->words[Inst->operands[0].offset];
        switch (Capability)
        {
        case SpvCapabilityClipDistance:
        case SpvCapabilityCullDistance:
        case SpvCapabilityImage1D:
        case SpvCapabilityImageCubeArray:
        case SpvCapabilityImageQuery:
        case SpvCapabilityInputAttachment:
        case SpvCapabilityInt16:
        case SpvCapabilityInt64:
        case SpvCapabilityFloat64:
        case SpvCapabilityImageBuffer:
        case SpvCapabilityMatrix:
        case SpvCapabilitySampled1D:
        case SpvCapabilitySampledBuffer:
        case SpvCapabilityShader:
        case SpvCapabilityStorageBuffer8BitAccess:
        case SpvCapabilityStorageBuffer16BitAccess:
        case SpvCapabilityStorageImageReadWithoutFormat:
        case SpvCapabilityStorageImageWriteWithoutFormat:
        case SpvCapabilityStorageInputOutput16:
        case SpvCapabilityStoragePushConstant8:
        case SpvCapabilityStoragePushConstant16:
        case SpvCapabilityStorageUniform16:
        case SpvCapabilityUniformAndStorageBuffer8BitAccess:
        case SpvCapabilityVariablePointers:
        case SpvCapabilityVariablePointersStorageBuffer:
          break;
        default:
          std::cerr << "Unimplemented capability: " << Capability << std::endl;
          abort();
        }
        break;
      }
      case SpvOpConstant:
      case SpvOpSpecConstant:
      {
        const Type *Ty = Mod->getType(Inst->type_id);

        Object Constant = Object(Ty);
        uint16_t Offset = Inst->operands[2].offset;
        switch (Ty->getSize())
        {
        case 2:
          Constant.set<uint16_t>(*(uint16_t *)(Inst->words + Offset));
          break;
        case 4:
          Constant.set<uint32_t>(Inst->words[Offset]);
          break;
        case 8:
          Constant.set<uint64_t>(*(uint64_t *)(Inst->words + Offset));
          break;
        default:
          std::cerr << "Unhandled OpConstant type size: " << Ty->getSize()
                    << std::endl;
          abort();
        }
        Mod->addObject(Inst->result_id, Constant);
        break;
      }
      case SpvOpConstantComposite:
      {
        const Type *Ty = Mod->getType(Inst->type_id);

        // Create composite object.
        Object Composite(Ty);

        // Set constituent values.
        for (uint32_t i = 2; i < Inst->num_operands; i++)
        {
          uint32_t Id = Inst->words[Inst->operands[i].offset];
          Composite.insert({i - 2}, Mod->getObject(Id));
        }

        // Add object to module.
        Mod->addObject(Inst->result_id, Composite);
        break;
      }
      case SpvOpConstantFalse:
      case SpvOpSpecConstantFalse:
      {
        const Type *Ty = Mod->getType(Inst->type_id);
        Mod->addObject(Inst->result_id, Object(Ty, false));
        break;
      }
      case SpvOpConstantNull:
      {
        // Create and add object.
        const Type *Ty = Mod->getType(Inst->type_id);
        Object Value(Ty);
        Value.zero();
        Mod->addObject(Inst->result_id, Value);
        break;
      }
      case SpvOpConstantTrue:
      case SpvOpSpecConstantTrue:
      {
        const Type *Ty = Mod->getType(Inst->type_id);
        Mod->addObject(Inst->result_id, Object(Ty, true));
        break;
      }
      case SpvOpDecorate:
      {
        uint32_t Target = Inst->words[Inst->operands[0].offset];
        uint32_t Decoration = Inst->words[Inst->operands[1].offset];
        switch (Decoration)
        {
        case SpvDecorationArrayStride:
          ArrayStrides[Target] = Inst->words[Inst->operands[2].offset];
          break;
        case SpvDecorationBinding:
        case SpvDecorationComponent:
        case SpvDecorationDescriptorSet:
        case SpvDecorationLocation:
          ObjectDecorations[Target].push_back(
              {Decoration, Inst->words[Inst->operands[2].offset]});
          break;
        case SpvDecorationAliased:
        case SpvDecorationCentroid:
        case SpvDecorationCoherent:
        case SpvDecorationFlat:
        case SpvDecorationNoContraction:
        case SpvDecorationNonReadable:
        case SpvDecorationNonWritable:
        case SpvDecorationNoPerspective:
        case SpvDecorationRestrict:
        case SpvDecorationVolatile:
          ObjectDecorations[Target].push_back({Decoration, 1});
          break;
        case SpvDecorationBlock:
        case SpvDecorationBufferBlock:
          // TODO: Need to handle these?
          break;
        case SpvDecorationBuiltIn:
        {
          switch (Inst->words[Inst->operands[2].offset])
          {
          case SpvBuiltInWorkgroupSize:
            Mod->setWorkgroupSizeId(Target);
            break;
          default:
            ObjectDecorations[Target].push_back(
                {Decoration, Inst->words[Inst->operands[2].offset]});
            break;
          }
          break;
        }
        case SpvDecorationRelaxedPrecision:
          break;
        case SpvDecorationSpecId:
          Mod->addSpecConstant(Inst->words[Inst->operands[2].offset], Target);
          break;
        default:
          std::cerr << "Unhandled decoration " << Decoration << std::endl;
          abort();
        }
        break;
      }
      case SpvOpEntryPoint:
      {
        uint32_t ExecutionModel = Inst->words[Inst->operands[0].offset];
        uint32_t Id = Inst->words[Inst->operands[1].offset];
        char *Name = (char *)(Inst->words + Inst->operands[2].offset);
        if (ExecutionModel != SpvExecutionModelGLCompute &&
            ExecutionModel != SpvExecutionModelVertex &&
            ExecutionModel != SpvExecutionModelFragment)
        {
          std::cerr << "Unimplemented execution model: " << ExecutionModel
                    << std::endl;
          abort();
        }

        // Save entry point specification for creation later.
        std::vector<uint32_t> Variables(Inst->words + Inst->operands[3].offset,
                                        Inst->words + Inst->operands[3].offset +
                                            Inst->num_operands - 3);
        EntryPoints[Id] = {Name, ExecutionModel, Variables};
        break;
      }
      case SpvOpExecutionMode:
      {
        uint32_t Entry = Inst->words[Inst->operands[0].offset];
        uint32_t Mode = Inst->words[Inst->operands[1].offset];
        switch (Mode)
        {
        case SpvExecutionModeLocalSize:
          Mod->addLocalSize(Entry,
                            Dim3(Inst->words + Inst->operands[2].offset));
          break;
        case SpvExecutionModeOriginUpperLeft:
          // TODO: Store this for later use?
          break;
        default:
          std::cerr << "Unimplemented execution mode: " << Mode << std::endl;
          abort();
        }
        break;
      }
      case SpvOpExtension:
      {
        char *Extension = (char *)(Inst->words + Inst->operands[0].offset);
        if (strcmp(Extension, "SPV_KHR_8bit_storage") &&
            strcmp(Extension, "SPV_KHR_16bit_storage") &&
            strcmp(Extension, "SPV_KHR_storage_buffer_storage_class") &&
            strcmp(Extension, "SPV_KHR_variable_pointers"))
        {
          std::cerr << "Unimplemented extension " << Extension << std::endl;
          abort();
        }
        break;
      }
      case SpvOpExtInstImport:
      {
        // TODO: Store the mapping from result ID to set for later use
        char *ExtInstSet = (char *)(Inst->words + Inst->operands[1].offset);
        if (strcmp(ExtInstSet, "GLSL.std.450"))
        {
          std::cerr << "Unrecognized extended instruction set " << ExtInstSet
                    << std::endl;
          abort();
        }
        break;
      }
      case SpvOpLine:
        // TODO: Do something with this
        break;
      case SpvOpMemberDecorate:
      {
        uint32_t Target = Inst->words[Inst->operands[0].offset];
        uint32_t Member = Inst->words[Inst->operands[1].offset];
        uint32_t Decoration = Inst->words[Inst->operands[2].offset];
        uint32_t Offset = Inst->operands[3].offset;
        switch (Decoration)
        {
        case SpvDecorationBuiltIn:
        case SpvDecorationMatrixStride:
        case SpvDecorationOffset:
          MemberDecorations[{Target, Member}][Decoration] = Inst->words[Offset];
          break;
        case SpvDecorationCoherent:
        case SpvDecorationColMajor:
        case SpvDecorationRowMajor:
        case SpvDecorationFlat:
        case SpvDecorationNonReadable:
        case SpvDecorationNonWritable:
        case SpvDecorationNoPerspective:
          MemberDecorations[{Target, Member}][Decoration] = 1;
          break;
        case SpvDecorationRelaxedPrecision:
          break;
        default:
          std::cerr << "Unhandled member decoration " << Decoration
                    << std::endl;
          abort();
        }
        break;
      }
      case SpvOpMemberName:
        // TODO: Do something with this
        break;
      case SpvOpMemoryModel:
      {
        uint32_t AddressingMode = Inst->words[Inst->operands[0].offset];
        uint32_t MemoryModel = Inst->words[Inst->operands[1].offset];
        if (AddressingMode != SpvAddressingModelLogical)
        {
          std::cerr << "Unrecognized addressing mode " << AddressingMode
                    << std::endl;
          abort();
        }
        if (MemoryModel != SpvMemoryModelGLSL450)
        {
          std::cerr << "Unrecognized memory model " << MemoryModel << std::endl;
          abort();
        }
        break;
      }
      case SpvOpModuleProcessed:
        break;
      case SpvOpName:
        // TODO: Do something with this
        break;
      case SpvOpNoLine:
        // TODO: Do something with this
        break;
      case SpvOpSpecConstantComposite:
      {
        const Type *ResultType = Mod->getType(Inst->type_id);

        // Build list of operands (the IDs of the constituents).
        std::vector<uint32_t> Operands(Inst->num_operands);
        for (int i = 0; i < Inst->num_operands; i++)
        {
          assert(Inst->operands[i].num_words == 1);
          Operands[i] = Inst->words[Inst->operands[i].offset];
        }

        // Create an OpCompositeConstruct instruction.
        Mod->addSpecConstantOp(new Instruction(SpvOpCompositeConstruct,
                                               Inst->num_operands,
                                               Operands.data(), ResultType));
        break;
      }
      case SpvOpSpecConstantOp:
      {
        const Type *ResultType = Mod->getType(Inst->type_id);
        uint16_t Opcode = Inst->words[Inst->operands[2].offset];

        // Build list of operands (skip opcode).
        std::vector<uint32_t> Operands;
        for (int i = 0; i < Inst->num_operands; i++)
        {
          if (i == 2)
            continue;
          assert(Inst->operands[i].num_words == 1);
          Operands.push_back(Inst->words[Inst->operands[i].offset]);
        }

        // Create the instruction.
        Mod->addSpecConstantOp(new Instruction(Opcode, Inst->num_operands - 1,
                                               Operands.data(), ResultType));
        break;
      }
      case SpvOpSource:
      case SpvOpSourceContinued:
        // TODO: Do something with these
        break;
      case SpvOpSourceExtension:
        break;
      case SpvOpString:
        // TODO: Do something with this
        break;
      case SpvOpTypeArray:
      {
        // Get array length.
        uint32_t LengthId = Inst->words[Inst->operands[2].offset];
        const Object &LengthObj = Mod->getObject(LengthId);
        uint32_t Length;
        switch (LengthObj.getType()->getBitWidth())
        {
        case 16:
          Length = (uint32_t)LengthObj.get<uint16_t>();
          break;
        case 32:
          Length = (uint32_t)LengthObj.get<uint32_t>();
          break;
        case 64:
          assert(LengthObj.get<uint64_t>() <= UINT32_MAX);
          Length = (uint32_t)LengthObj.get<uint64_t>();
          break;
        default:
          std::cerr << "Invalid array length bitwidth" << std::endl;
          abort();
        }

        const Type *ElemType =
            Mod->getType(Inst->words[Inst->operands[1].offset]);
        uint32_t ArrayStride = (uint32_t)ElemType->getSize();
        if (ArrayStrides.count(Inst->result_id))
          ArrayStride = ArrayStrides[Inst->result_id];
        Mod->addType(Inst->result_id,
                     Type::getArray(ElemType, Length, ArrayStride));
        break;
      }
      case SpvOpTypeBool:
      {
        Mod->addType(Inst->result_id, Type::getBool());
        break;
      }
      case SpvOpTypeFloat:
      {
        uint32_t Width = Inst->words[Inst->operands[1].offset];
        Mod->addType(Inst->result_id, Type::getFloat(Width));
        break;
      }
      case SpvOpTypeInt:
      {
        uint32_t Width = Inst->words[Inst->operands[1].offset];
        Mod->addType(Inst->result_id, Type::getInt(Width));
        break;
      }
      case SpvOpTypeFunction:
      {
        uint32_t ReturnType = Inst->words[Inst->operands[1].offset];
        std::vector<const Type *> ArgTypes;
        for (int i = 2; i < Inst->num_operands; i++)
        {
          uint32_t ArgType = Inst->words[Inst->operands[i].offset];
          ArgTypes.push_back(Mod->getType(ArgType));
        }
        Mod->addType(Inst->result_id,
                     Type::getFunction(Mod->getType(ReturnType), ArgTypes));
        break;
      }
      case SpvOpTypeImage:
      {
        const Type *SampledType =
            Mod->getType(Inst->words[Inst->operands[1].offset]);
        uint32_t Dim = Inst->words[Inst->operands[2].offset];
        uint32_t Depth = Inst->words[Inst->operands[3].offset];
        uint32_t Arrayed = Inst->words[Inst->operands[4].offset];
        uint32_t MS = Inst->words[Inst->operands[5].offset];
        uint32_t Sampled = Inst->words[Inst->operands[6].offset];
        uint32_t Format = Inst->words[Inst->operands[7].offset];
        Mod->addType(Inst->result_id,
                     Type::getImage(SampledType, Dim, Depth, Arrayed, MS,
                                    Sampled, Format));
        break;
      }
      case SpvOpTypeMatrix:
      {
        const Type *ColumnType =
            Mod->getType(Inst->words[Inst->operands[1].offset]);
        uint32_t NumColumns = Inst->words[Inst->operands[2].offset];
        Mod->addType(Inst->result_id, Type::getMatrix(ColumnType, NumColumns));
        break;
      }
      case SpvOpTypePointer:
      {
        uint32_t StorageClass = Inst->words[Inst->operands[1].offset];
        const Type *ElemType =
            Mod->getType(Inst->words[Inst->operands[2].offset]);
        uint32_t ArrayStride = (uint32_t)ElemType->getSize();
        if (ArrayStrides.count(Inst->result_id))
          ArrayStride = ArrayStrides[Inst->result_id];
        Mod->addType(Inst->result_id,
                     Type::getPointer(StorageClass, ElemType, ArrayStride));
        break;
      }
      case SpvOpTypeRuntimeArray:
      {
        const Type *ElemType =
            Mod->getType(Inst->words[Inst->operands[1].offset]);
        uint32_t ArrayStride = (uint32_t)ElemType->getSize();
        if (ArrayStrides.count(Inst->result_id))
          ArrayStride = ArrayStrides[Inst->result_id];
        Mod->addType(Inst->result_id,
                     Type::getRuntimeArray(ElemType, ArrayStride));
        break;
      }
      case SpvOpTypeSampledImage:
      {
        const Type *ImageType =
            Mod->getType(Inst->words[Inst->operands[1].offset]);
        Mod->addType(Inst->result_id, Type::getSampledImage(ImageType));
        break;
      }
      case SpvOpTypeSampler:
      {
        Mod->addType(Inst->result_id, Type::getSampler());
        break;
      }
      case SpvOpTypeStruct:
      {
        StructElementTypeList ElemTypes;
        for (int i = 1; i < Inst->num_operands; i++)
        {
          // Get member type and decorations if present.
          uint32_t ElemTypeId = Inst->words[Inst->operands[i].offset];
          const Type *ElemType = Mod->getType(ElemTypeId);
          if (MemberDecorations.count({Inst->result_id, i - 1}))
            ElemTypes.push_back(
                {ElemType, MemberDecorations[{Inst->result_id, i - 1}]});
          else
            ElemTypes.push_back({ElemType, {}});
        }
        Mod->addType(Inst->result_id, Type::getStruct(ElemTypes));
        break;
      }
      case SpvOpTypeVector:
      {
        const Type *ElemType =
            Mod->getType(Inst->words[Inst->operands[1].offset]);
        uint32_t ElemCount = Inst->words[Inst->operands[2].offset];
        Mod->addType(Inst->result_id, Type::getVector(ElemType, ElemCount));
        break;
      }
      case SpvOpTypeVoid:
      {
        Mod->addType(Inst->result_id, Type::getVoid());
        break;
      }
      case SpvOpUndef:
        Mod->addObject(Inst->result_id, Object(Mod->getType(Inst->type_id)));
        break;
      case SpvOpVariable:
      {
        // Create variable.
        uint32_t Initializer =
            ((Inst->num_operands > 3) ? Inst->words[Inst->operands[3].offset]
                                      : 0);
        Variable *Var = new Variable(Inst->result_id,
                                     Mod->getType(Inst->type_id), Initializer);

        // Add decorations if present.
        if (ObjectDecorations.count(Inst->result_id))
        {
          for (auto &VD : ObjectDecorations[Inst->result_id])
            Var->addDecoration(VD.first, VD.second);
        }

        Mod->addVariable(Var);
        break;
      }
      default:
        std::cerr << "Unhandled instruction: "
                  << Instruction::opcodeToString(Inst->opcode) << " ("
                  << Inst->opcode << ")" << std::endl;
        abort();
      }
    }
  };

  /// Returns the Module that has been built.
  std::shared_ptr<Module> getModule() { return Mod; }

private:
  /// Internal ModuleBuilder variables.
  ///\{
  std::shared_ptr<Module> Mod;
  std::unique_ptr<Function> CurrentFunction;
  std::unique_ptr<Block> CurrentBlock;
  Instruction *PreviousInstruction;
  std::map<uint32_t, uint32_t> ArrayStrides;
  std::map<std::pair<uint32_t, uint32_t>, std::map<uint32_t, uint32_t>>
      MemberDecorations;
  std::map<uint32_t, std::vector<std::pair<uint32_t, uint32_t>>>
      ObjectDecorations;

  struct EntryPointSpec
  {
    std::string Name;
    uint32_t ExecutionModel;
    std::vector<uint32_t> Variables;
  };
  std::map<uint32_t, EntryPointSpec> EntryPoints;
  ///\}
};

/// Callback for SPIRV-Tools parsing a SPIR-V header.
spv_result_t HandleHeader(void *user_data, spv_endianness_t endian,
                          uint32_t /* magic */, uint32_t version,
                          uint32_t generator, uint32_t id_bound,
                          uint32_t schema)
{
  ((ModuleBuilder *)user_data)->init(id_bound);
  return SPV_SUCCESS;
}

/// Callback for SPIRV-Tools parsing a SPIR-V instruction.
spv_result_t
HandleInstruction(void *user_data,
                  const spv_parsed_instruction_t *parsed_instruction)
{
  ((ModuleBuilder *)user_data)->processInstruction(parsed_instruction);
  return SPV_SUCCESS;
}

Module::Module(uint32_t IdBound)
{
  this->IdBound = IdBound;
  this->Objects.resize(IdBound);
  WorkgroupSizeId = 0;
}

Module::~Module()
{
  for (auto Op : SpecConstantOps)
    delete Op;

  for (auto EP : EntryPoints)
    delete EP;

  for (auto Var : Variables)
    delete Var;
}

void Module::addEntryPoint(EntryPoint *EP)
{
  assert(getEntryPoint(EP->getName(), EP->getExecutionModel()) == nullptr);
  EntryPoints.push_back(EP);
}

void Module::addFunction(std::unique_ptr<Function> Func)
{
  assert(Functions.count(Func->getId()) == 0);
  Functions[Func->getId()] = std::move(Func);
}

void Module::addLocalSize(uint32_t Entry, Dim3 LocalSize)
{
  assert(LocalSizes.count(Entry) == 0);
  LocalSizes[Entry] = LocalSize;
}

void Module::addObject(uint32_t Id, const Object &Obj)
{
  assert(Id < Objects.size());
  Objects[Id] = Obj;
}

void Module::addSpecConstant(uint32_t SpecId, uint32_t ResultId)
{
  // TODO: Allow the same SpecId to apply to multiple results.
  assert(SpecConstants.count(SpecId) == 0);
  SpecConstants[SpecId] = ResultId;
}

void Module::addSpecConstantOp(Instruction *Op)
{
  SpecConstantOps.push_back(Op);
}

void Module::addType(uint32_t Id, std::unique_ptr<Type> Ty)
{
  assert(!Types.count(Id));
  Types[Id] = std::move(Ty);
}

const EntryPoint *Module::getEntryPoint(const std::string &Name,
                                        uint32_t ExecutionModel) const
{
  auto Itr = std::find_if(EntryPoints.begin(), EntryPoints.end(), [&](auto EP) {
    return EP->getName() == Name && EP->getExecutionModel() == ExecutionModel;
  });
  if (Itr == EntryPoints.end())
    return nullptr;
  return *Itr;
}

const Function *Module::getFunction(uint32_t Id) const
{
  if (!Functions.count(Id))
    return nullptr;
  return Functions.at(Id).get();
}

Dim3 Module::getLocalSize(uint32_t Entry) const
{
  if (LocalSizes.count(Entry))
    return LocalSizes.at(Entry);
  else
    return Dim3(1, 1, 1);
}

const Object &Module::getObject(uint32_t Id) const { return Objects.at(Id); }

const std::vector<Object> &Module::getObjects() const { return Objects; }

uint32_t Module::getSpecConstant(uint32_t SpecId) const
{
  if (SpecConstants.count(SpecId) == 0)
    return 0;
  return SpecConstants.at(SpecId);
}

const std::vector<Instruction *> &Module::getSpecConstantOps() const
{
  return SpecConstantOps;
}

const Type *Module::getType(uint32_t Id) const
{
  if (Types.count(Id) == 0)
    return nullptr;
  return Types.at(Id).get();
}

std::shared_ptr<Module> Module::load(spvtools::Context &SPVContext,
                                     const uint32_t *Words, size_t NumWords)
{
  spv_diagnostic Diagnostic = nullptr;

  // Validate binary.
  if (spvValidateBinary(SPVContext.CContext(), Words, NumWords, &Diagnostic))
  {
    spvDiagnosticPrint(Diagnostic);
    return nullptr;
  }

  // Parse binary.
  ModuleBuilder MB;
  spvBinaryParse(SPVContext.CContext(), &MB, Words, NumWords, HandleHeader,
                 HandleInstruction, &Diagnostic);
  if (Diagnostic)
  {
    spvDiagnosticPrint(Diagnostic);
    return nullptr;
  }

  return MB.getModule();
}

std::shared_ptr<Module> Module::load(const std::string &FileName)
{
  // Open file.
  FILE *SPVFile = fopen(FileName.c_str(), "rb");
  if (!SPVFile)
  {
    std::cerr << "Failed to open '" << FileName << "'" << std::endl;
    return nullptr;
  }

  // Read file data.
  std::vector<uint8_t> Bytes;
  fseek(SPVFile, 0, SEEK_END);
  long NumBytes = ftell(SPVFile);
  Bytes.resize(NumBytes);
  fseek(SPVFile, 0, SEEK_SET);
  fread(Bytes.data(), 1, NumBytes, SPVFile);
  fclose(SPVFile);

  return Module::load(Bytes);
}

std::shared_ptr<Module> Module::load(const std::vector<uint8_t> &Bytes)
{
  spv_target_env target_env = SPV_ENV_UNIVERSAL_1_6;
  // spv_target_env target_env = SPV_ENV_VULKAN_1_0; // TODO
  auto NumBytes = Bytes.size();

  // Check for SPIR-V magic number.
  if (((uint32_t *)Bytes.data())[0] == 0x07230203)
  {
    spvtools::Context SPVContext(target_env); // TODO?
    return load(SPVContext, (uint32_t *)Bytes.data(), NumBytes / 4);
  }

  // Assume file is in textual SPIR-V format.
  // Assemble it to a SPIR-V binary in memory.
  spv_binary Binary;
  spv_diagnostic Diagnostic = nullptr;

  auto hasPrefix = [&Bytes](const char *prefix, size_t start = 0) -> bool {
    auto Byte = Bytes.begin() + start;
    for (; *prefix && Byte < Bytes.end(); Byte++, prefix++)
    {
      if (*prefix != *Byte)
      {
        return false;
      }
    }
    return true;
  };
  // TODO robust?
  // TODO std::string that knows its length?
  const auto SPIRV_HEADER = "; SPIR-V\n";
  const auto VERSION_HEADER = "; Version: ";
  if (hasPrefix(SPIRV_HEADER) &&
      hasPrefix(VERSION_HEADER, strlen(SPIRV_HEADER)))
  {
    const auto N = strlen(SPIRV_HEADER) + strlen(VERSION_HEADER);
    auto matchVersion = [&hasPrefix, N](const char *version) -> bool {
      return hasPrefix(version, N);
    };
    if (matchVersion("1.0"))
      target_env = SPV_ENV_UNIVERSAL_1_0;
    else if (matchVersion("1.1"))
      target_env = SPV_ENV_UNIVERSAL_1_1;
    else if (matchVersion("1.2"))
      target_env = SPV_ENV_UNIVERSAL_1_2;
    else if (matchVersion("1.3"))
      target_env = SPV_ENV_UNIVERSAL_1_3;
    else if (matchVersion("1.4"))
      target_env = SPV_ENV_UNIVERSAL_1_4;
    else if (matchVersion("1.5"))
      target_env = SPV_ENV_UNIVERSAL_1_5;
    else if (matchVersion("1.6"))
      target_env = SPV_ENV_UNIVERSAL_1_6;

    // otherwise, go with default
  }

  spvtools::Context SPVContext(target_env);
  auto result =
      spvTextToBinary(SPVContext.CContext(), (const char *)Bytes.data(),
                      NumBytes, &Binary, &Diagnostic);
  if (Diagnostic)
  {
    spvDiagnosticPrint(Diagnostic);
    spvDiagnosticDestroy(Diagnostic);
    if (result)
      spvBinaryDestroy(Binary);
    return nullptr;
  }

  // Load and return Module.
  std::shared_ptr<Module> M = load(SPVContext, Binary->code, Binary->wordCount);
  spvBinaryDestroy(Binary);
  return M;
}

} // namespace talvos
