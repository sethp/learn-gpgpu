// Copyright (c) 2018 the Talvos developers. All rights reserved.
//
// This file is distributed under a three-clause BSD license. For full license
// terms please see the LICENSE file distributed with this source code.

/// \file Invocation.cpp
/// This file defines the Invocation class.

#include <array>
#include <cassert>
#include <cmath>
#include <iostream>
#include <sstream>

#include <spirv/unified1/GLSL.std.450.h>
#include <spirv/unified1/spirv.h>

#include "talvos/Block.h"
#include "talvos/Device.h"
#include "talvos/EntryPoint.h"
#include "talvos/Function.h"
#include "talvos/Image.h"
#include "talvos/Instruction.h"
#include "talvos/Invocation.h"
#include "talvos/Memory.h"
#include "talvos/Module.h"
#include "talvos/PipelineStage.h"
#include "talvos/Type.h"
#include "talvos/Variable.h"
#include "talvos/Workgroup.h"

/// Get scalar operand at index \p Index with type \p Type.
#define OP(Index, Type) Objects[Inst->getOperand(Index)].get<Type>()

namespace talvos
{

Invocation::Invocation(Device &Dev, const std::vector<Object> &InitialObjects)
    : Dev(Dev)
{
  CurrentInstruction = nullptr;
  PrivateMemory = nullptr;
  PipelineMemory = nullptr;
  Objects = InitialObjects;
}

Invocation::Invocation(Device &Dev, const PipelineStage &Stage,
                       const std::vector<Object> &InitialObjects,
                       std::shared_ptr<Memory> PipelineMemory, Workgroup *Group,
                       Dim3 GlobalId)
    : Dev(Dev), Group(Group), GlobalId(GlobalId), PipelineMemory(PipelineMemory)
{
  PrivateMemory = new Memory(Dev, MemoryScope::Invocation);

  AtBarrier = false;
  Discarded = false;
  CurrentModule = Stage.getModule();
  CurrentFunction = Stage.getEntryPoint()->getFunction();
  moveToBlock(CurrentFunction->getFirstBlockId());

  // Clone initial object values.
  Objects = InitialObjects;

  // Copy workgroup variable pointer values.
  if (Group)
  {
    for (auto V : Group->getVariables())
      Objects[V.first] = V.second;
  }

  // Set up private variables.
  for (auto V : CurrentModule->getVariables())
  {
    const Type *Ty = V->getType();
    if (Ty->getStorageClass() != SpvStorageClassPrivate)
      continue;

    // Allocate and initialize variable in private memory.
    uint64_t NumBytes = Ty->getElementType()->getSize();
    uint64_t Address = PrivateMemory->allocate(NumBytes);
    Objects[V->getId()] = Object(Ty, Address);
    if (V->getInitializer())
      Objects[V->getInitializer()].store(*PrivateMemory, Address);
  }

  Dev.reportInvocationBegin(this);
}

Invocation::~Invocation() { delete PrivateMemory; }

void Invocation::execute(const talvos::Instruction *Inst)
{
  // Dispatch instruction to handler method.
  uint16_t Opcode = Inst->getOpcode();
  switch (Opcode)
  {
#define DISPATCH(Op, Func)                                                     \
  case Op:                                                                     \
    execute##Func(Inst);                                                       \
    break
#define NOP(Op)                                                                \
  case Op:                                                                     \
    break

    DISPATCH(SpvOpAccessChain, AccessChain);
    DISPATCH(SpvOpAll, All);
    DISPATCH(SpvOpAny, Any);
    DISPATCH(SpvOpAtomicAnd, AtomicOp<uint32_t>);
    DISPATCH(SpvOpAtomicCompareExchange, AtomicCompareExchange);
    DISPATCH(SpvOpAtomicExchange, AtomicOp<uint32_t>);
    DISPATCH(SpvOpAtomicIAdd, AtomicOp<uint32_t>);
    DISPATCH(SpvOpAtomicIDecrement, AtomicOp<uint32_t>);
    DISPATCH(SpvOpAtomicIIncrement, AtomicOp<uint32_t>);
    DISPATCH(SpvOpAtomicISub, AtomicOp<uint32_t>);
    DISPATCH(SpvOpAtomicLoad, AtomicOp<uint32_t>);
    DISPATCH(SpvOpAtomicOr, AtomicOp<uint32_t>);
    DISPATCH(SpvOpAtomicSMax, AtomicOp<int32_t>);
    DISPATCH(SpvOpAtomicSMin, AtomicOp<int32_t>);
    DISPATCH(SpvOpAtomicStore, AtomicOp<uint32_t>);
    DISPATCH(SpvOpAtomicUMax, AtomicOp<uint32_t>);
    DISPATCH(SpvOpAtomicUMin, AtomicOp<uint32_t>);
    DISPATCH(SpvOpAtomicXor, AtomicOp<uint32_t>);
    DISPATCH(SpvOpBitcast, Bitcast);
    DISPATCH(SpvOpBitwiseAnd, BitwiseAnd);
    DISPATCH(SpvOpBitwiseOr, BitwiseOr);
    DISPATCH(SpvOpBitwiseXor, BitwiseXor);
    DISPATCH(SpvOpBranch, Branch);
    DISPATCH(SpvOpBranchConditional, BranchConditional);
    DISPATCH(SpvOpCompositeConstruct, CompositeConstruct);
    DISPATCH(SpvOpCompositeExtract, CompositeExtract);
    DISPATCH(SpvOpCompositeInsert, CompositeInsert);
    DISPATCH(SpvOpControlBarrier, ControlBarrier);
    DISPATCH(SpvOpConvertFToS, ConvertFToS);
    DISPATCH(SpvOpConvertFToU, ConvertFToU);
    DISPATCH(SpvOpConvertSToF, ConvertSToF);
    DISPATCH(SpvOpConvertUToF, ConvertUToF);
    DISPATCH(SpvOpCopyMemory, CopyMemory);
    DISPATCH(SpvOpCopyObject, CopyObject);
    DISPATCH(SpvOpDot, Dot);
    DISPATCH(SpvOpExtInst, ExtInst);
    DISPATCH(SpvOpFAdd, FAdd);
    DISPATCH(SpvOpFConvert, FConvert);
    DISPATCH(SpvOpFDiv, FDiv);
    DISPATCH(SpvOpFMod, FMod);
    DISPATCH(SpvOpFMul, FMul);
    DISPATCH(SpvOpFNegate, FNegate);
    DISPATCH(SpvOpFOrdEqual, FOrdEqual);
    DISPATCH(SpvOpFOrdGreaterThan, FOrdGreaterThan);
    DISPATCH(SpvOpFOrdGreaterThanEqual, FOrdGreaterThanEqual);
    DISPATCH(SpvOpFOrdLessThan, FOrdLessThan);
    DISPATCH(SpvOpFOrdLessThanEqual, FOrdLessThanEqual);
    DISPATCH(SpvOpFOrdNotEqual, FOrdNotEqual);
    DISPATCH(SpvOpFRem, FRem);
    DISPATCH(SpvOpFSub, FSub);
    DISPATCH(SpvOpFunctionCall, FunctionCall);
    DISPATCH(SpvOpFUnordEqual, FUnordEqual);
    DISPATCH(SpvOpFUnordGreaterThan, FUnordGreaterThan);
    DISPATCH(SpvOpFUnordGreaterThanEqual, FUnordGreaterThanEqual);
    DISPATCH(SpvOpFUnordLessThan, FUnordLessThan);
    DISPATCH(SpvOpFUnordLessThanEqual, FUnordLessThanEqual);
    DISPATCH(SpvOpFUnordNotEqual, FUnordNotEqual);
    DISPATCH(SpvOpIAdd, IAdd);
    DISPATCH(SpvOpIEqual, IEqual);
    DISPATCH(SpvOpImage, Image);
    DISPATCH(SpvOpImageFetch, ImageRead);
    DISPATCH(SpvOpImageQuerySize, ImageQuerySize);
    DISPATCH(SpvOpImageQuerySizeLod, ImageQuerySize);
    DISPATCH(SpvOpImageRead, ImageRead);
    DISPATCH(SpvOpImageSampleExplicitLod, ImageSampleExplicitLod);
    DISPATCH(SpvOpImageWrite, ImageWrite);
    DISPATCH(SpvOpIMul, IMul);
    DISPATCH(SpvOpInBoundsAccessChain, AccessChain);
    DISPATCH(SpvOpINotEqual, INotEqual);
    DISPATCH(SpvOpIsInf, IsInf);
    DISPATCH(SpvOpIsNan, IsNan);
    DISPATCH(SpvOpISub, ISub);
    DISPATCH(SpvOpKill, Kill);
    DISPATCH(SpvOpLoad, Load);
    DISPATCH(SpvOpLogicalEqual, LogicalEqual);
    DISPATCH(SpvOpLogicalNotEqual, LogicalNotEqual);
    DISPATCH(SpvOpLogicalOr, LogicalOr);
    DISPATCH(SpvOpLogicalAnd, LogicalAnd);
    DISPATCH(SpvOpLogicalNot, LogicalNot);
    DISPATCH(SpvOpMatrixTimesScalar, MatrixTimesScalar);
    DISPATCH(SpvOpMatrixTimesVector, MatrixTimesVector);
    DISPATCH(SpvOpNot, Not);
    DISPATCH(SpvOpPhi, Phi);
    DISPATCH(SpvOpPtrAccessChain, AccessChain);
    DISPATCH(SpvOpReturn, Return);
    DISPATCH(SpvOpReturnValue, ReturnValue);
    DISPATCH(SpvOpSampledImage, SampledImage);
    DISPATCH(SpvOpSConvert, SConvert);
    DISPATCH(SpvOpSDiv, SDiv);
    DISPATCH(SpvOpSelect, Select);
    DISPATCH(SpvOpSGreaterThan, SGreaterThan);
    DISPATCH(SpvOpSGreaterThanEqual, SGreaterThanEqual);
    DISPATCH(SpvOpShiftLeftLogical, ShiftLeftLogical);
    DISPATCH(SpvOpShiftRightArithmetic, ShiftRightArithmetic);
    DISPATCH(SpvOpShiftRightLogical, ShiftRightLogical);
    DISPATCH(SpvOpSLessThan, SLessThan);
    DISPATCH(SpvOpSLessThanEqual, SLessThanEqual);
    DISPATCH(SpvOpSMod, SMod);
    DISPATCH(SpvOpSNegate, SNegate);
    DISPATCH(SpvOpSRem, SRem);
    DISPATCH(SpvOpStore, Store);
    DISPATCH(SpvOpSwitch, Switch);
    DISPATCH(SpvOpUConvert, UConvert);
    DISPATCH(SpvOpUDiv, UDiv);
    DISPATCH(SpvOpUGreaterThan, UGreaterThan);
    DISPATCH(SpvOpUGreaterThanEqual, UGreaterThanEqual);
    DISPATCH(SpvOpULessThan, ULessThan);
    DISPATCH(SpvOpULessThanEqual, ULessThanEqual);
    DISPATCH(SpvOpUMod, UMod);
    DISPATCH(SpvOpUndef, Undef);
    DISPATCH(SpvOpUnreachable, Unreachable);
    DISPATCH(SpvOpVariable, Variable);
    DISPATCH(SpvOpVectorExtractDynamic, VectorExtractDynamic);
    DISPATCH(SpvOpVectorInsertDynamic, VectorInsertDynamic);
    DISPATCH(SpvOpVectorShuffle, VectorShuffle);
    DISPATCH(SpvOpVectorTimesMatrix, VectorTimesMatrix);
    DISPATCH(SpvOpVectorTimesScalar, VectorTimesScalar);

    NOP(SpvOpNop);
    NOP(SpvOpLine);
    NOP(SpvOpLoopMerge);
    NOP(SpvOpMemoryBarrier);
    NOP(SpvOpNoLine);
    NOP(SpvOpSelectionMerge);

#undef DISPATCH
#undef NOP

  default:
    Dev.reportError("Unimplemented instruction", true);
  }
}

void Invocation::executeAccessChain(const Instruction *Inst)
{
  // Base pointer.
  uint32_t Id = Inst->getOperand(1);
  Object &Base = Objects[Inst->getOperand(2)];

  // Ensure base pointer is valid.
  if (!Base)
  {
    // Check for buffer variable matching base pointer ID.
    for (auto V : CurrentModule->getVariables())
    {
      if (V->getId() == Inst->getOperand(2))
      {
        // Report error for missing descriptor set entry.
        if (V->isBufferVariable())
        {
          std::stringstream Err;
          Err << "Invalid base pointer for descriptor set entry ("
              << V->getDecoration(SpvDecorationDescriptorSet) << ","
              << V->getDecoration(SpvDecorationBinding) << ")";
          Dev.reportError(Err.str());
        }
        else
        {
          Dev.reportError("Unresolved OpVariable pointer", true);
        }

        // Set result pointer to null.
        Objects[Id] = Object(Inst->getResultType(), (uint64_t)0);
        return;
      }
    }
    assert(false && "Invalid base pointer for AccessChain");
  }

  uint64_t Result = Base.get<uint64_t>();
  const Type *Ty = Base.getType()->getElementType();

  // Initialize matrix layout.
  PtrMatrixLayout MatrixLayout;
  if (Ty->isMatrix() || Ty->isVector())
    MatrixLayout = Base.getMatrixLayout();

  // Offset of the first index operand.
  uint32_t FirstIndexOperand = 3;

  // Perform initial dereference for element index for OpPtrAccessChain.
  if (Inst->getOpcode() == SpvOpPtrAccessChain)
  {
    // TODO: Need to handle this?
    assert(!Base.getDescriptorElements());

    FirstIndexOperand = 4;
    switch (Objects[Inst->getOperand(3)].getType()->getSize())
    {
    case 2:
      Result += Base.getType()->getElementOffset(OP(3, uint16_t));
      break;
    case 4:
      Result += Base.getType()->getElementOffset(OP(3, uint32_t));
      break;
    case 8:
      Result += Base.getType()->getElementOffset(OP(3, uint64_t));
      break;
    default:
      Dev.reportError("Unhandled index size", true);
      return;
    }
  }

  // Loop over indices.
  for (uint32_t i = FirstIndexOperand; i < Inst->getNumOperands(); i++)
  {
    uint64_t Index;
    const Object &IndexObj = Objects[Inst->getOperand(i)];
    switch (IndexObj.getType()->getSize())
    {
    case 2:
      Index = IndexObj.get<uint16_t>();
      break;
    case 4:
      Index = IndexObj.get<uint32_t>();
      break;
    case 8:
      Index = IndexObj.get<uint64_t>();
      break;
    default:
      Dev.reportError("Unhandled index size", true);
      return;
    }

    const Type *ElemTy = Ty->getElementType(Index);

    if (Base.getDescriptorElements() && i == FirstIndexOperand)
    {
      // Special case for arrays of descriptors.
      if (Index < Ty->getElementCount())
      {
        Result = Base.getDescriptorElements()[Index].Address;
      }
      else
      {
        Dev.reportError("Descriptor array element exceeds array size", false);
        Result = 0;
      }
    }
    else if (Ty->isMatrix() && MatrixLayout)
    {
      // Special case for matrix pointers with non-default layouts.
      if (MatrixLayout.Order == PtrMatrixLayout::COL_MAJOR)
        Result += Index * MatrixLayout.Stride;
      else
        Result += Index * ElemTy->getElementType()->getSize();
    }
    else if (Ty->isVector() && MatrixLayout)
    {
      // Special case for vector pointers with non-default layouts.
      if (MatrixLayout.Order == PtrMatrixLayout::COL_MAJOR)
        Result += Index * ElemTy->getSize();
      else
        Result += Index * MatrixLayout.Stride;
    }
    else
    {
      Result += Ty->getElementOffset(Index);
    }

    // Check for structure member decorations that affect memory layout.
    if (Ty->getTypeId() == Type::STRUCT)
    {
      auto &Decorations = Ty->getStructMemberDecorations((uint32_t)Index);
      if (Decorations.count(SpvDecorationMatrixStride))
      {
        // Track matrix layout.
        MatrixLayout.Stride = Decorations.at(SpvDecorationMatrixStride);
        if (Decorations.count(SpvDecorationColMajor))
        {
          MatrixLayout.Order = PtrMatrixLayout::COL_MAJOR;
        }
        else
        {
          assert(Decorations.count(SpvDecorationRowMajor));
          MatrixLayout.Order = PtrMatrixLayout::ROW_MAJOR;
        }
      }
    }

    Ty = ElemTy;
  }

  Objects[Id] = Object(Inst->getResultType(), Result);

  // Set matrix layout for result pointer if necessary.
  if (MatrixLayout && (Ty->isVector() || Ty->isMatrix()))
    Objects[Id].setMatrixLayout(MatrixLayout);
}

void Invocation::executeAll(const Instruction *Inst)
{
  uint32_t Id = Inst->getOperand(1);
  Object Result(Inst->getResultType(), true);
  const Object &Vector = Objects[Inst->getOperand(2)];
  for (uint32_t i = 0; i < Vector.getType()->getElementCount(); i++)
  {
    if (!Vector.get<bool>(i))
    {
      Result.set(false);
      break;
    }
  }
  Objects[Id] = Result;
}

void Invocation::executeAny(const Instruction *Inst)
{
  uint32_t Id = Inst->getOperand(1);
  Object Result(Inst->getResultType(), false);
  const Object &Vector = Objects[Inst->getOperand(2)];
  for (uint32_t i = 0; i < Vector.getType()->getElementCount(); i++)
  {
    if (Vector.get<bool>(i))
    {
      Result.set(true);
      break;
    }
  }
  Objects[Id] = Result;
}

template <typename T> void Invocation::executeAtomicOp(const Instruction *Inst)
{
  assert(Inst->getOpcode() != SpvOpAtomicCompareExchange);

  // Get index of pointer operand.
  uint32_t PtrOp = (Inst->getOpcode() == SpvOpAtomicStore) ? 0 : 2;

  Object Pointer = Objects[Inst->getOperand(PtrOp)];
  uint32_t Scope = Objects[Inst->getOperand(PtrOp + 1)].get<uint32_t>();
  uint32_t Semantics = Objects[Inst->getOperand(PtrOp + 2)].get<uint32_t>();

  // Get value operand if present.
  T Value = 0;
  if (Inst->getNumOperands() > (PtrOp + 3))
    Value = Objects[Inst->getOperand(PtrOp + 3)].get<T>();

  // Perform atomic operation.
  Memory &Mem = getMemory(Pointer.getType()->getStorageClass());
  T Result = Mem.atomic<T>(Pointer.get<uint64_t>(), Inst->getOpcode(), Scope,
                           Semantics, Value);

  // Create result if necessary.
  if (PtrOp == 2)
    Objects[Inst->getOperand(1)] = Object(Inst->getResultType(), Result);
}

void Invocation::executeAtomicCompareExchange(const Instruction *Inst)
{
  Object Pointer = Objects[Inst->getOperand(2)];
  uint32_t Scope = Objects[Inst->getOperand(3)].get<uint32_t>();
  uint32_t EqualSemantics = Objects[Inst->getOperand(4)].get<uint32_t>();
  uint32_t UnequalSemantics = Objects[Inst->getOperand(5)].get<uint32_t>();
  uint32_t Value = Objects[Inst->getOperand(6)].get<uint32_t>();
  uint32_t Comparator = Objects[Inst->getOperand(7)].get<uint32_t>();

  Memory &Mem = getMemory(Pointer.getType()->getStorageClass());
  uint32_t Result =
      Mem.atomicCmpXchg(Pointer.get<uint64_t>(), Scope, EqualSemantics,
                        UnequalSemantics, Value, Comparator);
  Objects[Inst->getOperand(1)] = Object(Inst->getResultType(), Result);
}

void Invocation::executeBitcast(const Instruction *Inst)
{
  const Object &Source = Objects[Inst->getOperand(2)];
  Object Result = Object(Inst->getResultType(), Source.getData());
  Objects[Inst->getOperand(1)] = Result;
}

void Invocation::executeBitwiseAnd(const Instruction *Inst)
{
  executeOpUInt<2>(Inst, [](auto A, auto B) -> decltype(A) { return A & B; });
}

void Invocation::executeBitwiseOr(const Instruction *Inst)
{
  executeOpUInt<2>(Inst, [](auto A, auto B) -> decltype(A) { return A | B; });
}

void Invocation::executeBitwiseXor(const Instruction *Inst)
{
  executeOpUInt<2>(Inst, [](auto A, auto B) -> decltype(A) { return A ^ B; });
}

void Invocation::executeBranch(const Instruction *Inst)
{
  moveToBlock(Inst->getOperand(0));
}

void Invocation::executeBranchConditional(const Instruction *Inst)
{
  bool Condition = OP(0, bool);
  moveToBlock(Inst->getOperand(Condition ? 1 : 2));
}

void Invocation::executeCompositeConstruct(const Instruction *Inst)
{
  uint32_t Id = Inst->getOperand(1);

  Object Result = Object(Inst->getResultType());

  // Set constituent values.
  for (uint32_t i = 2; i < Inst->getNumOperands(); i++)
  {
    uint32_t Id = Inst->getOperand(i);
    Result.insert({i - 2}, Objects[Id]);
  }

  Objects[Id] = Result;
}

void Invocation::executeCompositeExtract(const Instruction *Inst)
{
  uint32_t Id = Inst->getOperand(1);
  // TODO: Handle indices of different sizes.
  std::vector<uint32_t> Indices(Inst->getOperands() + 3,
                                Inst->getOperands() + Inst->getNumOperands());
  Objects[Id] = Objects[Inst->getOperand(2)].extract(Indices);
}

void Invocation::executeCompositeInsert(const Instruction *Inst)
{
  uint32_t Id = Inst->getOperand(1);
  Object &Element = Objects[Inst->getOperand(2)];
  // TODO: Handle indices of different sizes.
  std::vector<uint32_t> Indices(Inst->getOperands() + 4,
                                Inst->getOperands() + Inst->getNumOperands());
  assert(Objects[Inst->getOperand(3)].getType()->isComposite());
  Objects[Id] = Objects[Inst->getOperand(3)];
  Objects[Id].insert(Indices, Element);
}

void Invocation::executeControlBarrier(const Instruction *Inst)
{
  // TODO: Handle other execution scopes
  assert(Objects[Inst->getOperand(0)].get<uint32_t>() == SpvScopeWorkgroup);
  AtBarrier = true;
}

void Invocation::executeConvertFToS(const Instruction *Inst)
{
  switch (Inst->getResultType()->getScalarType()->getBitWidth())
  {
  case 16:
    executeOpFP<1>(Inst, [](auto A) -> int16_t { return (int16_t)A; });
    break;
  case 32:
    executeOpFP<1>(Inst, [](auto A) -> int32_t { return (int32_t)A; });
    break;
  case 64:
    executeOpFP<1>(Inst, [](auto A) -> int64_t { return (int64_t)A; });
    break;
  default:
    assert(false && "Unhandled integer size for OpConvertFToS");
  }
}

void Invocation::executeConvertFToU(const Instruction *Inst)
{
  switch (Inst->getResultType()->getScalarType()->getBitWidth())
  {
  case 16:
    executeOpFP<1>(Inst, [](auto A) -> uint16_t { return (uint16_t)A; });
    break;
  case 32:
    executeOpFP<1>(Inst, [](auto A) -> uint32_t { return (uint32_t)A; });
    break;
  case 64:
    executeOpFP<1>(Inst, [](auto A) -> uint64_t { return (uint64_t)A; });
    break;
  default:
    assert(false && "Unhandled integer size for OpConvertFToU");
  }
}

void Invocation::executeConvertSToF(const Instruction *Inst)
{
  switch (Inst->getResultType()->getScalarType()->getBitWidth())
  {
  case 32:
    executeOpSInt<1>(Inst, [](auto A) -> float { return (float)A; });
    break;
  case 64:
    executeOpSInt<1>(Inst, [](auto A) -> double { return (double)A; });
    break;
  default:
    assert(false && "Unhandled floating point size for OpConvertUToF");
  }
}

void Invocation::executeConvertUToF(const Instruction *Inst)
{
  switch (Inst->getResultType()->getScalarType()->getBitWidth())
  {
  case 32:
    executeOpUInt<1>(Inst, [](auto A) -> float { return (float)A; });
    break;
  case 64:
    executeOpUInt<1>(Inst, [](auto A) -> double { return (double)A; });
    break;
  default:
    assert(false && "Unhandled floating point size for OpConvertUToF");
  }
}

void Invocation::executeCopyMemory(const Instruction *Inst)
{
  const Object &Dst = Objects[Inst->getOperand(0)];
  const Object &Src = Objects[Inst->getOperand(1)];

  const Type *DstType = Dst.getType();
  const Type *SrcType = Src.getType();
  assert(DstType->getElementType() == SrcType->getElementType());

  Memory &DstMem = getMemory(DstType->getStorageClass());
  Memory &SrcMem = getMemory(SrcType->getStorageClass());

  uint64_t DstAddress = Dst.get<uint64_t>();
  uint64_t SrcAddress = Src.get<uint64_t>();
  uint64_t NumBytes = DstType->getElementType()->getSize();
  Memory::copy(DstAddress, DstMem, SrcAddress, SrcMem, NumBytes);
}

void Invocation::executeCopyObject(const Instruction *Inst)
{
  Objects[Inst->getOperand(1)] = Objects[Inst->getOperand(2)];
}

void Invocation::executeDot(const Instruction *Inst)
{
  Object &A = Objects[Inst->getOperand(2)];
  Object &B = Objects[Inst->getOperand(3)];
  switch (Inst->getResultType()->getBitWidth())
  {
  case 32:
  {
    float Result = 0.f;
    for (uint32_t i = 0; i < A.getType()->getElementCount(); i++)
      Result += A.get<float>(i) * B.get<float>(i);
    Objects[Inst->getOperand(1)] = Object(Inst->getResultType(), Result);
    break;
  }
  case 64:
  {
    double Result = 0.0;
    for (uint32_t i = 0; i < A.getType()->getElementCount(); i++)
      Result += A.get<double>(i) * B.get<double>(i);
    Objects[Inst->getOperand(1)] = Object(Inst->getResultType(), Result);
    break;
  }
  default:
    assert(false && "Unhandled floating point size for OpDot");
  }
}

void Invocation::executeExtInst(const Instruction *Inst)
{
  // TODO: Currently assumes extended instruction set is GLSL.std.450
  uint32_t ExtInst = Inst->getOperand(3);
  switch (ExtInst)
  {
  case GLSLstd450Acos:
    executeOpFP<1, 4>(Inst, [](auto X) -> decltype(X) { return acos(X); });
    break;
  case GLSLstd450Acosh:
    executeOpFP<1, 4>(Inst, [](auto X) -> decltype(X) { return acosh(X); });
    break;
  case GLSLstd450Asin:
    executeOpFP<1, 4>(Inst, [](auto X) -> decltype(X) { return asin(X); });
    break;
  case GLSLstd450Asinh:
    executeOpFP<1, 4>(Inst, [](auto X) -> decltype(X) { return asinh(X); });
    break;
  case GLSLstd450Atan:
    executeOpFP<1, 4>(Inst, [](auto X) -> decltype(X) { return atan(X); });
    break;
  case GLSLstd450Atanh:
    executeOpFP<1, 4>(Inst, [](auto X) -> decltype(X) { return atanh(X); });
    break;
  case GLSLstd450Atan2:
    executeOpFP<2, 4>(
        Inst, [](auto Y, auto X) -> decltype(X) { return atan2(Y, X); });
    break;
  case GLSLstd450Cos:
    executeOpFP<1, 4>(Inst, [](auto X) -> decltype(X) { return cos(X); });
    break;
  case GLSLstd450Cosh:
    executeOpFP<1, 4>(Inst, [](auto X) -> decltype(X) { return cosh(X); });
    break;
  case GLSLstd450FAbs:
    executeOpFP<1, 4>(Inst, [](auto X) -> decltype(X) { return fabs(X); });
    break;
  case GLSLstd450Fma:
  {
    executeOpFP<3, 4>(
        Inst, [](auto A, auto B, auto C) -> decltype(A) { return A * B + C; });
    break;
  }
  case GLSLstd450Floor:
    executeOpFP<1, 4>(Inst, [](auto X) -> decltype(X) { return floor(X); });
    break;
  case GLSLstd450InverseSqrt:
    executeOpFP<1, 4>(Inst,
                      [](auto X) -> decltype(X) { return 1.f / sqrt(X); });
    break;
  case GLSLstd450NClamp:
    executeOpFP<3, 4>(Inst, [](auto X, auto Min, auto Max) -> decltype(X) {
      return fmin(fmax(X, Min), Max);
    });
    break;
  case GLSLstd450FMax:
  case GLSLstd450NMax:
    executeOpFP<2, 4>(Inst,
                      [](auto X, auto Y) -> decltype(X) { return fmax(X, Y); });
    break;
  case GLSLstd450FMin:
  case GLSLstd450NMin:
    executeOpFP<2, 4>(Inst,
                      [](auto X, auto Y) -> decltype(X) { return fmin(X, Y); });
    break;
  case GLSLstd450Sin:
    executeOpFP<1, 4>(Inst, [](auto X) -> decltype(X) { return sin(X); });
    break;
  case GLSLstd450Sinh:
    executeOpFP<1, 4>(Inst, [](auto X) -> decltype(X) { return sinh(X); });
    break;
  case GLSLstd450Sqrt:
    executeOpFP<1, 4>(Inst, [](auto X) -> decltype(X) { return sqrt(X); });
    break;
  case GLSLstd450Tan:
    executeOpFP<1, 4>(Inst, [](auto X) -> decltype(X) { return tan(X); });
    break;
  case GLSLstd450Tanh:
    executeOpFP<1, 4>(Inst, [](auto X) -> decltype(X) { return tanh(X); });
    break;
  default:
    Dev.reportError("Unimplemented GLSL.std.450 extended instruction", true);
  }
}

void Invocation::executeFAdd(const Instruction *Inst)
{
  executeOpFP<2>(Inst, [](auto A, auto B) -> decltype(A) { return A + B; });
}

void Invocation::executeFConvert(const Instruction *Inst)
{
  switch (Inst->getResultType()->getBitWidth())
  {
  case 32:
    executeOpFP<1>(Inst, [](auto A) -> float { return (float)A; });
    break;
  case 64:
    executeOpFP<1>(Inst, [](auto A) -> double { return (double)A; });
    break;
  default:
    assert(false && "Unhandled floating point size for OpFConvert");
  }
}

void Invocation::executeFDiv(const Instruction *Inst)
{
  executeOpFP<2>(Inst, [](auto A, auto B) -> decltype(A) { return A / B; });
}

void Invocation::executeFMod(const Instruction *Inst)
{
  executeOpFP<2>(Inst, [](auto A, auto B) -> decltype(A) {
    return A - (B * floor(A / B));
  });
}

void Invocation::executeFMul(const Instruction *Inst)
{
  executeOpFP<2>(Inst, [](auto A, auto B) -> decltype(A) { return A * B; });
}

void Invocation::executeFNegate(const Instruction *Inst)
{
  executeOpFP<1>(Inst, [](auto A) -> decltype(A) { return -A; });
}

void Invocation::executeFOrdEqual(const Instruction *Inst)
{
  executeOpFP<2>(Inst, [](auto A, auto B) -> bool {
    return A == B && !std::isunordered(A, B);
  });
}

void Invocation::executeFOrdGreaterThan(const Instruction *Inst)
{
  executeOpFP<2>(Inst, [](auto A, auto B) -> bool {
    return A > B && !std::isunordered(A, B);
  });
}

void Invocation::executeFOrdGreaterThanEqual(const Instruction *Inst)
{
  executeOpFP<2>(Inst, [](auto A, auto B) -> bool {
    return A >= B && !std::isunordered(A, B);
  });
}

void Invocation::executeFOrdLessThan(const Instruction *Inst)
{
  executeOpFP<2>(Inst, [](auto A, auto B) -> bool {
    return A < B && !std::isunordered(A, B);
  });
}

void Invocation::executeFOrdLessThanEqual(const Instruction *Inst)
{
  executeOpFP<2>(Inst, [](auto A, auto B) -> bool {
    return A <= B && !std::isunordered(A, B);
  });
}

void Invocation::executeFOrdNotEqual(const Instruction *Inst)
{
  executeOpFP<2>(Inst, [](auto A, auto B) -> bool {
    return A != B && !std::isunordered(A, B);
  });
}

void Invocation::executeFRem(const Instruction *Inst)
{
  executeOpFP<2>(Inst,
                 [](auto A, auto B) -> decltype(A) { return fmod(A, B); });
}

void Invocation::executeFSub(const Instruction *Inst)
{
  executeOpFP<2>(Inst, [](auto A, auto B) -> decltype(A) { return A - B; });
}

void Invocation::executeFunctionCall(const Instruction *Inst)
{
  const Function *Func = CurrentModule->getFunction(Inst->getOperand(2));

  // Copy function parameters.
  assert(Inst->getNumOperands() == Func->getNumParams() + 3);
  for (int i = 3; i < Inst->getNumOperands(); i++)
    Objects[Func->getParamId(i - 3)] = Objects[Inst->getOperand(i)];

  // Create call stack entry.
  StackEntry SE;
  SE.CallInst = Inst;
  SE.CallFunc = CurrentFunction;
  SE.CallBlock = CurrentBlock;
  CallStack.push_back(SE);

  // Move to first block of callee function.
  CurrentFunction = Func;
  moveToBlock(CurrentFunction->getFirstBlockId());
}

void Invocation::executeFUnordEqual(const Instruction *Inst)
{
  executeOpFP<2>(Inst, [](auto A, auto B) -> bool {
    return A == B || std::isunordered(A, B);
  });
}

void Invocation::executeFUnordGreaterThan(const Instruction *Inst)
{
  executeOpFP<2>(Inst, [](auto A, auto B) -> bool {
    return A > B || std::isunordered(A, B);
  });
}

void Invocation::executeFUnordGreaterThanEqual(const Instruction *Inst)
{
  executeOpFP<2>(Inst, [](auto A, auto B) -> bool {
    return A >= B || std::isunordered(A, B);
  });
}

void Invocation::executeFUnordLessThan(const Instruction *Inst)
{
  executeOpFP<2>(Inst, [](auto A, auto B) -> bool {
    return A < B || std::isunordered(A, B);
  });
}

void Invocation::executeFUnordLessThanEqual(const Instruction *Inst)
{
  executeOpFP<2>(Inst, [](auto A, auto B) -> bool {
    return A <= B || std::isunordered(A, B);
  });
}

void Invocation::executeFUnordNotEqual(const Instruction *Inst)
{
  executeOpFP<2>(Inst, [](auto A, auto B) -> bool {
    return A != B || std::isunordered(A, B);
  });
}

void Invocation::executeIAdd(const Instruction *Inst)
{
  executeOpUInt<2>(Inst, [](auto A, auto B) -> decltype(A) { return A + B; });
}

void Invocation::executeIEqual(const Instruction *Inst)
{
  executeOpUInt<2>(Inst, [](auto A, auto B) -> bool { return A == B; });
}

void Invocation::executeImage(const Instruction *Inst)
{
  // Extract image object from a sampled image.
  const Object &SampledImageObj = Objects[Inst->getOperand(2)];
  const SampledImage *SI = (const SampledImage *)(SampledImageObj.getData());
  Objects[Inst->getOperand(1)] =
      Object(SampledImageObj.getType()->getElementType(),
             (const uint8_t *)&(SI->Image));
}

void Invocation::executeImageQuerySize(const Instruction *Inst)
{
  // Get image view object.
  const Object &ImageObj = Objects[Inst->getOperand(2)];
  const ImageView *Image = *(const ImageView **)(ImageObj.getData());

  Object Result(Inst->getResultType());
  assert(Result.getType()->getScalarType()->getBitWidth() == 32);

  // Get mip level (if explicit).
  uint32_t Level = 0;
  if (Inst->getOpcode() == SpvOpImageQuerySizeLod)
    Level = Objects[Inst->getOperand(3)].get<uint32_t>();

  // Get size in each dimension.
  uint32_t ArraySizeIndex;
  switch (ImageObj.getType()->getDimensionality())
  {
  case SpvDim1D:
  case SpvDimBuffer:
    Result.set<uint32_t>(Image->getWidth(Level), 0);
    ArraySizeIndex = 1;
    break;
  case SpvDim2D:
  case SpvDimCube:
  case SpvDimRect:
    Result.set<uint32_t>(Image->getWidth(Level), 0);
    Result.set<uint32_t>(Image->getHeight(Level), 1);
    ArraySizeIndex = 2;
    break;
  case SpvDim3D:
    Result.set<uint32_t>(Image->getWidth(Level), 0);
    Result.set<uint32_t>(Image->getHeight(Level), 1);
    Result.set<uint32_t>(Image->getDepth(Level), 2);
    ArraySizeIndex = 3;
    break;
  default:
    Dev.reportError("Unhandled image dimensionality", true);
    break;
  }

  // Get number of array layers.
  if (ImageObj.getType()->isArrayedImage())
  {
    if (ImageObj.getType()->getDimensionality() == SpvDimCube)
      Result.set<uint32_t>(Image->getNumArrayLayers() / 6, ArraySizeIndex);
    else
      Result.set<uint32_t>(Image->getNumArrayLayers(), ArraySizeIndex);
  }

  Objects[Inst->getOperand(1)] = Result;
}

void Invocation::executeImageRead(const Instruction *Inst)
{
  // Get image view object.
  const Object &ImageObj = Objects[Inst->getOperand(2)];
  const ImageView *Image = *(const ImageView **)(ImageObj.getData());

  // TODO: Handle subpass data dimensionality
  assert(ImageObj.getType()->getDimensionality() != SpvDimSubpassData);

  // Get coordinate operand.
  const Object &Coord = Objects[Inst->getOperand(3)];
  const Type *CoordType = Coord.getType();
  uint32_t NumCoords = CoordType->getElementCount();
  assert(NumCoords <= 3);

  // Last coordinate is array layer if required.
  uint32_t Layer = 0;
  if (ImageObj.getType()->isArrayedImage() ||
      ImageObj.getType()->getDimensionality() == SpvDimCube)
    Layer = Coord.get<uint32_t>(--NumCoords);

  // Extract coordinates.
  uint32_t X = Coord.get<uint32_t>(0);
  uint32_t Y = (NumCoords > 1) ? Coord.get<uint32_t>(1) : 0;
  uint32_t Z = (NumCoords > 2) ? Coord.get<uint32_t>(2) : 0;
  uint32_t Level = 0;

  // Handle optional image operands.
  if (Inst->getNumOperands() > 4)
  {
    uint32_t OpIdx = 5;
    uint32_t OperandMask = Inst->getOperand(4);

    if (OperandMask & SpvImageOperandsLodMask)
    {
      Level = Objects[Inst->getOperand(OpIdx++)].get<uint32_t>();
      OperandMask ^= SpvImageOperandsLodMask;
    }

    // Check for any remaining values after all supported operands handled.
    if (OperandMask)
      Dev.reportError("Unhandled image operand mask", true);
    assert(OpIdx == Inst->getNumOperands());
  }

  // Read texel from image.
  Image::Texel T;
  Image->read(T, X, Y, Z, Layer, Level);
  Objects[Inst->getOperand(1)] = T.toObject(Inst->getResultType());
}

void Invocation::executeImageSampleExplicitLod(const Instruction *Inst)
{
  // Get sampler and image view objects.
  const Object &SampledImageObj = Objects[Inst->getOperand(2)];
  const SampledImage *SI = (const SampledImage *)(SampledImageObj.getData());
  const ImageView *Image = SI->Image;
  const Sampler *Sampler = SI->Sampler;
  const Type *ImageType = SampledImageObj.getType()->getElementType();

  // Get coordinate operand.
  const Object &Coord = Objects[Inst->getOperand(3)];
  const Type *CoordType = Coord.getType();
  uint32_t NumCoords = CoordType->getElementCount();
  assert(CoordType->getScalarType()->isFloat());
  assert(NumCoords <= 3);

  // Last coordinate is array layer if required.
  float Layer = 0;
  if (ImageType->isArrayedImage() ||
      ImageType->getDimensionality() == SpvDimCube)
    Layer = Coord.get<float>(--NumCoords);

  // Extract coordinates.
  float X = Coord.get<float>(0);
  float Y = (NumCoords > 1) ? Coord.get<float>(1) : 0;
  float Z = (NumCoords > 2) ? Coord.get<float>(2) : 0;

  // TODO: Handle additional operands
  // TODO: Handle Lod properly
  assert(Inst->getNumOperands() == 6);
  assert(Inst->getOperand(4) == SpvImageOperandsLodMask);
  assert(Objects[Inst->getOperand(5)].get<float>() == 0);

  // Sample texel from image.
  Image::Texel Texel;
  Sampler->sample(Image, Texel, X, Y, Z, Layer);
  Objects[Inst->getOperand(1)] = Texel.toObject(Inst->getResultType());
}

void Invocation::executeImageWrite(const Instruction *Inst)
{
  // Get image view object.
  const Object &ImageObj = Objects[Inst->getOperand(0)];
  const ImageView *Image = *(const ImageView **)(ImageObj.getData());

  // TODO: Handle additional operands
  assert(Inst->getNumOperands() == 3);

  // Get coordinate operand.
  const Object &Coord = Objects[Inst->getOperand(1)];
  const Type *CoordType = Coord.getType();
  uint32_t NumCoords = CoordType->getElementCount();
  assert(NumCoords <= 3);

  // Last coordinate is array layer if required.
  uint32_t Layer = 0;
  if (ImageObj.getType()->isArrayedImage() ||
      ImageObj.getType()->getDimensionality() == SpvDimCube)
    Layer = Coord.get<uint32_t>(--NumCoords);

  // Extract coordinates.
  uint32_t X = Coord.get<uint32_t>(0);
  uint32_t Y = (NumCoords > 1) ? Coord.get<uint32_t>(1) : 0;
  uint32_t Z = (NumCoords > 2) ? Coord.get<uint32_t>(2) : 0;

  // Write texel to image.
  const Object &Texel = Objects[Inst->getOperand(2)];
  Image->write(Texel, X, Y, Z, Layer);
}

void Invocation::executeIMul(const Instruction *Inst)
{
  executeOpUInt<2>(Inst, [](auto A, auto B) -> decltype(A) { return A * B; });
}

void Invocation::executeINotEqual(const Instruction *Inst)
{
  executeOpUInt<2>(Inst, [](auto A, auto B) -> bool { return A != B; });
}

void Invocation::executeIsInf(const Instruction *Inst)
{
  executeOpFP<1>(Inst, [](auto A) -> bool { return std::isinf(A); });
}

void Invocation::executeIsNan(const Instruction *Inst)
{
  executeOpFP<1>(Inst, [](auto A) -> bool { return std::isnan(A); });
}

void Invocation::executeISub(const Instruction *Inst)
{
  executeOpUInt<2>(Inst, [](auto A, auto B) -> decltype(A) { return A - B; });
}

void Invocation::executeKill(const Instruction *Inst)
{
  Discarded = true;
  CurrentInstruction = nullptr;
}

void Invocation::executeLoad(const Instruction *Inst)
{
  uint32_t Id = Inst->getOperand(1);
  const Object &Src = Objects[Inst->getOperand(2)];
  Memory &Mem = getMemory(Src.getType()->getStorageClass());
  Objects[Id] = Object::load(Inst->getResultType(), Mem, Src);
}

void Invocation::executeLogicalAnd(const Instruction *Inst)
{
  executeOp<bool, 2>(Inst, [](bool A, bool B) { return A && B; });
}

void Invocation::executeLogicalEqual(const Instruction *Inst)
{
  executeOp<bool, 2>(Inst, [](bool A, bool B) { return A == B; });
}

void Invocation::executeLogicalNot(const Instruction *Inst)
{
  executeOp<bool, 1>(Inst, [](bool A) { return !A; });
}

void Invocation::executeLogicalNotEqual(const Instruction *Inst)
{
  executeOp<bool, 2>(Inst, [](bool A, bool B) { return A != B; });
}

void Invocation::executeLogicalOr(const Instruction *Inst)
{
  executeOp<bool, 2>(Inst, [](bool A, bool B) { return A || B; });
}

void Invocation::executeMatrixTimesScalar(const Instruction *Inst)
{
  Object Matrix = Objects[Inst->getOperand(2)];
  Object Scalar = Objects[Inst->getOperand(3)];
  const Type *MatrixType = Matrix.getType();
  const Type *VectorType = MatrixType->getElementType();
  const Type *ScalarType = VectorType->getElementType();
  assert(ScalarType->isFloat());

  for (uint32_t col = 0; col < MatrixType->getElementCount(); col++)
  {
    for (uint32_t row = 0; row < VectorType->getElementCount(); row++)
    {
      Object Element = Matrix.extract({col, row});
      switch (ScalarType->getBitWidth())
      {
      case 32:
        Element.set(Element.get<float>() * Scalar.get<float>());
        break;
      case 64:
        Element.set(Element.get<double>() * Scalar.get<double>());
        break;
      default:
        Dev.reportError("Unhandled floating point size", true);
        break;
      }
      Matrix.insert({col, row}, Element);
    }
  }

  Objects[Inst->getOperand(1)] = Matrix;
}

void Invocation::executeMatrixTimesVector(const Instruction *Inst)
{
  Object Matrix = Objects[Inst->getOperand(2)];
  Object Vector = Objects[Inst->getOperand(3)];
  const Type *MatrixType = Matrix.getType();
  const Type *ColumnType = MatrixType->getElementType();
  const Type *ScalarType = Inst->getResultType()->getElementType();
  assert(ScalarType->isFloat());

  Object Result(Inst->getResultType());
  for (uint32_t row = 0; row < ColumnType->getElementCount(); row++)
  {
    switch (ScalarType->getBitWidth())
    {
    case 32:
    {
      float R = 0.f;
      for (uint32_t col = 0; col < MatrixType->getElementCount(); col++)
        R += Vector.get<float>(col) * Matrix.extract({col, row}).get<float>();
      Result.set(R, row);
      break;
    }
    case 64:
    {
      double R = 0.f;
      for (uint32_t col = 0; col < MatrixType->getElementCount(); col++)
        R += Vector.get<double>(col) * Matrix.extract({col, row}).get<double>();
      Result.set(R, row);
      break;
    }
    default:
      Dev.reportError("Unhandled floating point size", true);
      break;
    }
  }
  Objects[Inst->getOperand(1)] = Result;
}

void Invocation::executeNot(const Instruction *Inst)
{
  executeOpUInt<1>(Inst, [](auto A) -> decltype(A) { return ~A; });
}

void Invocation::executePhi(const Instruction *Inst)
{
  uint32_t Id = Inst->getOperand(1);

  assert(PreviousBlock);
  for (int i = 2; i < Inst->getNumOperands(); i += 2)
  {
    assert(i + 1 < Inst->getNumOperands());
    if (Inst->getOperand(i + 1) == PreviousBlock)
    {
      PhiTemps.push_back({Id, Objects[Inst->getOperand(i)]});
      return;
    }
  }
  assert(false && "no matching predecessor block for OpPhi");
}

void Invocation::executeReturn(const Instruction *Inst)
{
  // If this is the entry function, do nothing.
  if (CallStack.empty())
    return;

  StackEntry SE = CallStack.back();
  CallStack.pop_back();

  // Release function scope allocations.
  for (uint64_t Address : SE.Allocations)
    PrivateMemory->release(Address);

  // Return to calling function.
  CurrentFunction = SE.CallFunc;
  CurrentBlock = SE.CallBlock;
  CurrentInstruction = SE.CallInst->next();
}

void Invocation::executeReturnValue(const Instruction *Inst)
{
  assert(!CallStack.empty());

  StackEntry SE = CallStack.back();
  CallStack.pop_back();

  // Set return value.
  Objects[SE.CallInst->getOperand(1)] = Objects[Inst->getOperand(0)];

  // Release function scope allocations.
  for (uint64_t Address : SE.Allocations)
    PrivateMemory->release(Address);

  // Return to calling function.
  CurrentFunction = SE.CallFunc;
  CurrentBlock = SE.CallBlock;
  CurrentInstruction = SE.CallInst->next();
}

void Invocation::executeSampledImage(const Instruction *Inst)
{
  // Get image view object.
  const Object &ImageObj = Objects[Inst->getOperand(2)];
  const ImageView *Image = *(const ImageView **)(ImageObj.getData());

  // Get sampler object.
  const Object &SamplerObj = Objects[Inst->getOperand(3)];
  const Sampler *Sampler = *(const talvos::Sampler **)(SamplerObj.getData());

  // Create and populate SampledImage structure.
  Object Result(Inst->getResultType());
  ((SampledImage *)(Result.getData()))->Image = Image;
  ((SampledImage *)(Result.getData()))->Sampler = Sampler;
  Objects[Inst->getOperand(1)] = Result;
}

void Invocation::executeSConvert(const Instruction *Inst)
{
  switch (Inst->getResultType()->getBitWidth())
  {
  case 16:
    executeOpSInt<1>(Inst, [](auto A) -> int16_t { return (int16_t)A; });
    break;
  case 32:
    executeOpSInt<1>(Inst, [](auto A) -> int32_t { return (int32_t)A; });
    break;
  case 64:
    executeOpSInt<1>(Inst, [](auto A) -> int64_t { return (int64_t)A; });
    break;
  default:
    assert(false && "Unhandled integer size for OpSConvert");
  }
}

void Invocation::executeSDiv(const Instruction *Inst)
{
  executeOpSInt<2>(Inst, [](auto A, auto B) -> decltype(A) { return A / B; });
}

void Invocation::executeSelect(const Instruction *Inst)
{
  uint32_t Id = Inst->getOperand(1);
  const Object &Condition = Objects[Inst->getOperand(2)];
  const Object &Object1 = Objects[Inst->getOperand(3)];
  const Object &Object2 = Objects[Inst->getOperand(4)];

  if (Condition.getType()->isScalar())
  {
    Objects[Id] = Condition.get<bool>() ? Object1 : Object2;
  }
  else
  {
    Object Result(Inst->getResultType());
    for (uint32_t i = 0; i < Result.getType()->getElementCount(); i++)
    {
      Result.insert({i}, Condition.get<bool>(i) ? Object1.extract({i})
                                                : Object2.extract({i}));
    }
    Objects[Id] = Result;
  }
}

void Invocation::executeSGreaterThan(const Instruction *Inst)
{
  executeOpSInt<2>(Inst, [](auto A, auto B) -> bool { return A > B; });
}

void Invocation::executeSGreaterThanEqual(const Instruction *Inst)
{
  executeOpSInt<2>(Inst, [](auto A, auto B) -> bool { return A >= B; });
}

void Invocation::executeShiftLeftLogical(const Instruction *Inst)
{
  executeOpUInt<2>(Inst, [](auto A, auto B) -> decltype(A) { return A << B; });
}

void Invocation::executeShiftRightArithmetic(const Instruction *Inst)
{
  executeOpSInt<2>(Inst, [](auto A, auto B) -> decltype(A) { return A >> B; });
}

void Invocation::executeShiftRightLogical(const Instruction *Inst)
{
  executeOpUInt<2>(Inst, [](auto A, auto B) -> decltype(A) { return A >> B; });
}

void Invocation::executeSLessThan(const Instruction *Inst)
{
  executeOpSInt<2>(Inst, [](auto A, auto B) -> bool { return A < B; });
}

void Invocation::executeSLessThanEqual(const Instruction *Inst)
{
  executeOpSInt<2>(Inst, [](auto A, auto B) -> bool { return A <= B; });
}

void Invocation::executeSMod(const Instruction *Inst)
{
  executeOpSInt<2>(Inst, [](auto A, auto B) -> decltype(A) {
    return (std::abs(A) % B) * (B < 0 ? -1 : 1);
  });
}

void Invocation::executeSNegate(const Instruction *Inst)
{
  executeOpSInt<1>(Inst, [](auto A) -> decltype(A) { return -A; });
}

void Invocation::executeSRem(const Instruction *Inst)
{
  executeOpSInt<2>(Inst, [](auto A, auto B) -> decltype(A) { return A % B; });
}

void Invocation::executeStore(const Instruction *Inst)
{
  uint32_t Id = Inst->getOperand(1);
  const Object &Dest = Objects[Inst->getOperand(0)];
  Memory &Mem = getMemory(Dest.getType()->getStorageClass());
  Objects[Id].store(Mem, Dest);
}

void Invocation::executeSwitch(const Instruction *Inst)
{
  const Object &Selector = Objects[Inst->getOperand(0)];

  // TODO: Handle other selector sizes
  if (Selector.getType()->getBitWidth() != 32)
    Dev.reportError("OpSwitch is only implemented for 32-bit selectors", true);

  for (uint32_t i = 2; i < Inst->getNumOperands(); i += 2)
  {
    if (Selector.get<uint32_t>() == Inst->getOperand(i))
    {
      moveToBlock(Inst->getOperand(i + 1));
      return;
    }
  }
  moveToBlock(Inst->getOperand(1));
}

void Invocation::executeUConvert(const Instruction *Inst)
{
  switch (Inst->getResultType()->getBitWidth())
  {
  case 16:
    executeOpUInt<1>(Inst, [](auto A) -> uint16_t { return (uint16_t)A; });
    break;
  case 32:
    executeOpUInt<1>(Inst, [](auto A) -> uint32_t { return (uint32_t)A; });
    break;
  case 64:
    executeOpUInt<1>(Inst, [](auto A) -> uint64_t { return (uint64_t)A; });
    break;
  default:
    assert(false && "Unhandled integer size for OpUConvert");
  }
}

void Invocation::executeUDiv(const Instruction *Inst)
{
  executeOpUInt<2>(Inst, [](auto A, auto B) -> decltype(A) { return A / B; });
}

void Invocation::executeUGreaterThan(const Instruction *Inst)
{
  executeOpUInt<2>(Inst, [](auto A, auto B) -> bool { return A > B; });
}

void Invocation::executeUGreaterThanEqual(const Instruction *Inst)
{
  executeOpUInt<2>(Inst, [](auto A, auto B) -> bool { return A >= B; });
}

void Invocation::executeULessThan(const Instruction *Inst)
{
  executeOpUInt<2>(Inst, [](auto A, auto B) -> bool { return A < B; });
}

void Invocation::executeULessThanEqual(const Instruction *Inst)
{
  executeOpUInt<2>(Inst, [](auto A, auto B) -> bool { return A <= B; });
}

void Invocation::executeUndef(const Instruction *Inst)
{
  Objects[Inst->getOperand(1)] = Object(Inst->getResultType());
}

void Invocation::executeUnreachable(const Instruction *Inst)
{
  Dev.reportError("OpUnreachable instruction executed", true);
}

void Invocation::executeUMod(const Instruction *Inst)
{
  executeOpUInt<2>(Inst, [](auto A, auto B) -> decltype(A) { return A % B; });
}

void Invocation::executeVariable(const Instruction *Inst)
{
  assert(Inst->getOperand(2) == SpvStorageClassFunction);

  uint32_t Id = Inst->getOperand(1);
  size_t AllocSize = Inst->getResultType()->getElementType()->getSize();
  uint64_t Address = PrivateMemory->allocate(AllocSize);
  Objects[Id] = Object(Inst->getResultType(), Address);

  // Initialize if necessary.
  if (Inst->getNumOperands() > 3)
    Objects[Inst->getOperand(3)].store(*PrivateMemory, Address);

  // Track function scope allocations.
  if (!CallStack.empty())
    CallStack.back().Allocations.push_back(Address);
}

void Invocation::executeVectorExtractDynamic(const Instruction *Inst)
{
  uint32_t Id = Inst->getOperand(1);
  uint16_t Index = 0;
  switch (Objects[Inst->getOperand(3)].getType()->getSize())
  {
  case 2:
    Index = OP(3, uint16_t);
    break;
  case 4:
    Index = (uint16_t)OP(3, uint32_t);
    break;
  case 8:
    Index = (uint16_t)OP(3, uint64_t);
    break;
  default:
    assert(false && "Unhandled index size in OpVectorExtractDynamic");
  }

  const Object &Vector = Objects[Inst->getOperand(2)];
  if (Index >= Vector.getType()->getElementCount())
    Dev.reportError("Vector index out of range");
  Objects[Id] = Vector.extract({Index});
}

void Invocation::executeVectorInsertDynamic(const Instruction *Inst)
{
  uint32_t Id = Inst->getOperand(1);
  uint16_t Index = 0;
  switch (Objects[Inst->getOperand(4)].getType()->getSize())
  {
  case 2:
    Index = OP(4, uint16_t);
    break;
  case 4:
    Index = (uint16_t)OP(4, uint32_t);
    break;
  case 8:
    Index = (uint16_t)OP(4, uint64_t);
    break;
  default:
    assert(false && "Unhandled index size in OpVectorInsertDynamic");
  }

  const Object &Vector = Objects[Inst->getOperand(2)];
  const Object &Component = Objects[Inst->getOperand(3)];
  if (Index >= Vector.getType()->getElementCount())
    Dev.reportError("Vector index out of range");
  Objects[Id] = Vector;
  Objects[Id].insert({Index}, Component);
}

void Invocation::executeVectorShuffle(const Instruction *Inst)
{
  uint32_t Id = Inst->getOperand(1);
  Object Result(Inst->getResultType());

  const Object &Vec1 = Objects[Inst->getOperand(2)];
  const Object &Vec2 = Objects[Inst->getOperand(3)];
  uint32_t Vec1Length = Vec1.getType()->getElementCount();

  for (uint32_t i = 0; i < Inst->getResultType()->getElementCount(); i++)
  {
    uint32_t Idx = Inst->getOperand(4 + i);
    if (Idx == 0xFFFFFFFF)
      ;
    else if (Idx < Vec1Length)
      Result.insert({i}, Vec1.extract({Idx}));
    else
      Result.insert({i}, Vec2.extract({Idx - Vec1Length}));
  }

  Objects[Id] = Result;
}

void Invocation::executeVectorTimesMatrix(const Instruction *Inst)
{
  Object Vector = Objects[Inst->getOperand(2)];
  Object Matrix = Objects[Inst->getOperand(3)];
  const Type *MatrixType = Matrix.getType();
  const Type *VectorType = Vector.getType();
  const Type *ScalarType = Inst->getResultType()->getElementType();
  assert(ScalarType->isFloat());

  Object Result(Inst->getResultType());
  for (uint32_t col = 0; col < MatrixType->getElementCount(); col++)
  {
    switch (ScalarType->getBitWidth())
    {
    case 32:
    {
      float R = 0.f;
      for (uint32_t row = 0; row < VectorType->getElementCount(); row++)
        R += Vector.get<float>(row) * Matrix.extract({col, row}).get<float>();
      Result.set(R, col);
      break;
    }
    case 64:
    {
      double R = 0.0;
      for (uint32_t row = 0; row < VectorType->getElementCount(); row++)
        R += Vector.get<double>(row) * Matrix.extract({col, row}).get<double>();
      Result.set(R, col);
      break;
    }
    default:
      Dev.reportError("Unhandled floating point size", true);
      break;
    }
  }
  Objects[Inst->getOperand(1)] = Result;
}

void Invocation::executeVectorTimesScalar(const Instruction *Inst)
{
  switch (Inst->getResultType()->getScalarType()->getBitWidth())
  {
  case 32:
  {
    float Scalar = Objects[Inst->getOperand(3)].get<float>();
    executeOp<float, 1>(Inst, [&](float A) { return A * Scalar; });
    break;
  }
  case 64:
  {
    double Scalar = Objects[Inst->getOperand(3)].get<double>();
    executeOp<double, 1>(Inst, [&](double A) { return A * Scalar; });
    break;
  }
  default:
    assert(false && "Unhandled floating point size for OpDot");
  }
}

Memory &Invocation::getMemory(uint32_t StorageClass)
{
  switch (StorageClass)
  {
  case SpvStorageClassPushConstant:
  case SpvStorageClassStorageBuffer:
  case SpvStorageClassUniform:
  case SpvStorageClassUniformConstant:
    return Dev.getGlobalMemory();
  case SpvStorageClassWorkgroup:
    assert(Group && "Not executing within a workgroup.");
    return Group->getLocalMemory();
  case SpvStorageClassInput:
  case SpvStorageClassOutput:
    return *PipelineMemory;
  case SpvStorageClassFunction:
  case SpvStorageClassPrivate:
    return *PrivateMemory;
  default:
    assert(false && "Unhandled storage class");
    abort();
  }
}

Object Invocation::getObject(uint32_t Id) const
{
  if (Id < Objects.size())
    return Objects[Id];
  else
    return Object();
}

Invocation::State Invocation::getState() const
{
  if (AtBarrier)
    return BARRIER;
  return CurrentInstruction ? READY : FINISHED;
}

void Invocation::moveToBlock(uint32_t Id)
{
  const Block *B = CurrentFunction->getBlock(Id);
  CurrentInstruction = B->getLabel().next();
  PreviousBlock = CurrentBlock;
  CurrentBlock = Id;
}

void Invocation::step()
{
  assert(getState() == READY);
  assert(CurrentInstruction);

  const Instruction *I = CurrentInstruction;

  if (!PhiTemps.empty() && I->getOpcode() != SpvOpPhi &&
      I->getOpcode() != SpvOpLine)
  {
    for (auto &P : PhiTemps)
      Objects[P.first] = std::move(P.second);
    PhiTemps.clear();
  }

  execute(I);

  // Move program counter to next instruction, unless a terminator instruction
  // was executed.
  if (I == CurrentInstruction)
    CurrentInstruction = CurrentInstruction->next();

  Dev.reportInstructionExecuted(this, I);

  if (getState() == FINISHED)
    Dev.reportInvocationComplete(this);
}

// Private helper functions for executing simple instructions.

template <typename OpTy, typename F>
static auto apply(const std::array<OpTy, 1> Operands, const F &Op)
{
  return Op(Operands[0]);
}

template <typename OpTy, typename F>
static auto apply(const std::array<OpTy, 2> Operands, const F &Op)
{
  return Op(Operands[0], Operands[1]);
}

template <typename OpTy, typename F>
static auto apply(const std::array<OpTy, 3> Operands, const F &Op)
{
  return Op(Operands[0], Operands[1], Operands[2]);
}

template <typename OpTy, unsigned N, unsigned Offset, typename F>
void Invocation::executeOp(const Instruction *Inst, const F &Op)
{
  uint32_t Id = Inst->getOperand(1);
  Object Result(Inst->getResultType());
  std::array<OpTy, N> Operands;

  // Loop over each vector component.
  for (uint32_t i = 0; i < Inst->getResultType()->getElementCount(); i++)
  {
    // Gather operands.
    for (unsigned j = 0; j < N; j++)
      Operands[j] = Objects[Inst->getOperand(Offset + j)].get<OpTy>(i);

    // Apply lambda and set result.
    Result.set(apply(Operands, Op), i);
  }

  Objects[Id] = Result;
}

template <unsigned N, unsigned Offset, typename F>
void Invocation::executeOpSInt(const Instruction *Inst, const F &&Op)
{
  const Type *OpType = Objects[Inst->getOperand(Offset)].getType();
  OpType = OpType->getScalarType();
  assert(OpType->isInt());
  switch (OpType->getBitWidth())
  {
  case 8:
    executeOp<int8_t, N, Offset>(Inst, Op);
    break;
  case 16:
    executeOp<int16_t, N, Offset>(Inst, Op);
    break;
  case 32:
    executeOp<int32_t, N, Offset>(Inst, Op);
    break;
  case 64:
    executeOp<int64_t, N, Offset>(Inst, Op);
    break;
  default:
    assert(false && "Unhandled binary operation integer width");
  }
}

template <unsigned N, unsigned Offset, typename F>
void Invocation::executeOpFP(const Instruction *Inst, const F &&Op)
{
  const Type *OpType = Objects[Inst->getOperand(Offset)].getType();
  OpType = OpType->getScalarType();
  assert(OpType->isFloat());
  switch (OpType->getBitWidth())
  {
  case 32:
    executeOp<float, N, Offset>(Inst, Op);
    break;
  case 64:
    executeOp<double, N, Offset>(Inst, Op);
    break;
  default:
    assert(false && "Unhandled binary operation floating point size");
  }
}

template <unsigned N, unsigned Offset, typename F>
void Invocation::executeOpUInt(const Instruction *Inst, const F &&Op)
{
  const Type *OpType = Objects[Inst->getOperand(Offset)].getType();
  OpType = OpType->getScalarType();
  assert(OpType->isInt());
  switch (OpType->getBitWidth())
  {
  case 8:
    executeOp<uint8_t, N>(Inst, Op);
    break;
  case 16:
    executeOp<uint16_t, N>(Inst, Op);
    break;
  case 32:
    executeOp<uint32_t, N>(Inst, Op);
    break;
  case 64:
    executeOp<uint64_t, N>(Inst, Op);
    break;
  default:
    assert(false && "Unhandled binary operation integer width");
  }
}

} // namespace talvos
