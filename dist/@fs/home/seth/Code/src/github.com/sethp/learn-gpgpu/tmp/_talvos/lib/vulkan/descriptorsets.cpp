// Copyright (c) 2018 the Talvos developers. All rights reserved.
//
// This file is distributed under a three-clause BSD license. For full license
// terms please see the LICENSE file distributed with this source code.

#include <cstring>
#include <functional>

#include "runtime.h"

#include "talvos/Device.h"
#include "talvos/Image.h"
#include "talvos/Memory.h"

VKAPI_ATTR VkResult VKAPI_CALL vkAllocateDescriptorSets(
    VkDevice device, const VkDescriptorSetAllocateInfo *pAllocateInfo,
    VkDescriptorSet *pDescriptorSets)
{
  for (uint32_t i = 0; i < pAllocateInfo->descriptorSetCount; i++)
  {
    const VkDescriptorSetLayout &Layout = pAllocateInfo->pSetLayouts[i];

    pDescriptorSets[i] = new VkDescriptorSet_T;
    pDescriptorSets[i]->Layout = Layout;

    // Set addresses for immutable samplers.
    for (auto IS : Layout->ImmutableSamplers)
    {
      uint32_t Binding = IS.first;
      for (uint32_t ArrayElement = 0;
           ArrayElement < Layout->BindingCounts[Binding]; ArrayElement++)
      {
        pDescriptorSets[i]->DescriptorSet[{Binding, ArrayElement}] = {
            IS.second[ArrayElement]->ObjectAddress, sizeof(talvos::Sampler *)};
      }
    }

    pAllocateInfo->descriptorPool->Pool.insert(pDescriptorSets[i]);
  }
  return VK_SUCCESS;
}

// Destroy a descriptor set and cleanup resources as necessary.
void destroyDescriptorSet(VkDevice Device, VkDescriptorSet Set)
{
  if (!Set)
    return;

  // Release allocations for combined image sampler objects.
  talvos::Memory &Mem = Device->Device->getGlobalMemory();
  for (auto Addr : Set->CombinedImageSamplers)
    Mem.release(Addr.second);

  delete Set;
}

VKAPI_ATTR void VKAPI_CALL vkCmdBindDescriptorSets(
    VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint,
    VkPipelineLayout layout, uint32_t firstSet, uint32_t descriptorSetCount,
    const VkDescriptorSet *pDescriptorSets, uint32_t dynamicOffsetCount,
    const uint32_t *pDynamicOffsets)
{
  // Build descriptor set map.
  talvos::DescriptorSetMap DSM;
  uint32_t OffsetIndex = 0;
  for (uint32_t i = 0; i < descriptorSetCount; i++)
  {
    DSM[firstSet + i] = pDescriptorSets[i]->DescriptorSet;

    // Add dynamic offsets.
    for (auto &Mapping : DSM[firstSet + i])
    {
      // Skip non-dynamic resources.
      const VkDescriptorType &Type =
          pDescriptorSets[i]->Layout->BindingTypes[Mapping.first.first];
      if (!(Type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC ||
            Type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC))
        continue;

      assert(OffsetIndex < dynamicOffsetCount);
      Mapping.second.Address += pDynamicOffsets[OffsetIndex];

      OffsetIndex++;
    }
  }

  assert(OffsetIndex == dynamicOffsetCount);

  // Bind descriptor sets to pipeline context.
  switch (pipelineBindPoint)
  {
  case VK_PIPELINE_BIND_POINT_GRAPHICS:
    commandBuffer->PipelineContext.bindGraphicsDescriptors(DSM);
    break;
  case VK_PIPELINE_BIND_POINT_COMPUTE:
    commandBuffer->PipelineContext.bindComputeDescriptors(DSM);
    break;
  default:
    assert(false && "invalid pipeline bind point");
  }
}

VKAPI_ATTR void VKAPI_CALL vkCmdPushConstants(VkCommandBuffer commandBuffer,
                                              VkPipelineLayout layout,
                                              VkShaderStageFlags stageFlags,
                                              uint32_t offset, uint32_t size,
                                              const void *pValues)
{
  commandBuffer->PipelineContext.setPushConstantData(offset, size,
                                                     (const uint8_t *)pValues);
}

VKAPI_ATTR void VKAPI_CALL vkCmdPushDescriptorSetKHR(
    VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint,
    VkPipelineLayout layout, uint32_t set, uint32_t descriptorWriteCount,
    const VkWriteDescriptorSet *pDescriptorWrites)
{
  TALVOS_ABORT_UNIMPLEMENTED;
}

VKAPI_ATTR void VKAPI_CALL vkCmdPushDescriptorSetWithTemplateKHR(
    VkCommandBuffer commandBuffer,
    VkDescriptorUpdateTemplate descriptorUpdateTemplate,
    VkPipelineLayout layout, uint32_t set, const void *pData)
{
  TALVOS_ABORT_UNIMPLEMENTED;
}

VKAPI_ATTR VkResult VKAPI_CALL vkCreateDescriptorPool(
    VkDevice device, const VkDescriptorPoolCreateInfo *pCreateInfo,
    const VkAllocationCallbacks *pAllocator, VkDescriptorPool *pDescriptorPool)
{
  (*pDescriptorPool) = new VkDescriptorPool_T;
  return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL vkCreateDescriptorSetLayout(
    VkDevice device, const VkDescriptorSetLayoutCreateInfo *pCreateInfo,
    const VkAllocationCallbacks *pAllocator, VkDescriptorSetLayout *pSetLayout)
{
  *pSetLayout = new VkDescriptorSetLayout_T;
  for (uint32_t i = 0; i < pCreateInfo->bindingCount; i++)
  {
    const VkDescriptorSetLayoutBinding &Binding = pCreateInfo->pBindings[i];
    (*pSetLayout)->BindingCounts[Binding.binding] = Binding.descriptorCount;
    (*pSetLayout)->BindingTypes[Binding.binding] = Binding.descriptorType;

    // Capture immutable sampler handles.
    if (Binding.pImmutableSamplers)
    {
      (*pSetLayout)->ImmutableSamplers[Binding.binding] =
          std::vector<VkSampler>(Binding.pImmutableSamplers,
                                 Binding.pImmutableSamplers +
                                     Binding.descriptorCount);
    }
  }
  return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL vkCreateDescriptorUpdateTemplate(
    VkDevice device, const VkDescriptorUpdateTemplateCreateInfo *pCreateInfo,
    const VkAllocationCallbacks *pAllocator,
    VkDescriptorUpdateTemplate *pDescriptorUpdateTemplate)
{
  *pDescriptorUpdateTemplate = new VkDescriptorUpdateTemplate_T;

  // Copy descriptor update entries.
  uint32_t EntryCount = pCreateInfo->descriptorUpdateEntryCount;
  (*pDescriptorUpdateTemplate)->EntryCount = EntryCount;
  (*pDescriptorUpdateTemplate)->Entries =
      new VkDescriptorUpdateTemplateEntry[EntryCount];
  memcpy((*pDescriptorUpdateTemplate)->Entries,
         pCreateInfo->pDescriptorUpdateEntries,
         EntryCount * sizeof(VkDescriptorUpdateTemplateEntry));

  return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL vkCreateDescriptorUpdateTemplateKHR(
    VkDevice device, const VkDescriptorUpdateTemplateCreateInfo *pCreateInfo,
    const VkAllocationCallbacks *pAllocator,
    VkDescriptorUpdateTemplate *pDescriptorUpdateTemplate)
{
  return vkCreateDescriptorUpdateTemplate(device, pCreateInfo, pAllocator,
                                          pDescriptorUpdateTemplate);
}

VKAPI_ATTR VkResult VKAPI_CALL vkCreatePipelineLayout(
    VkDevice device, const VkPipelineLayoutCreateInfo *pCreateInfo,
    const VkAllocationCallbacks *pAllocator, VkPipelineLayout *pPipelineLayout)
{
  // TODO: Implement?
  *pPipelineLayout = new VkPipelineLayout_T;
  return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL
vkDestroyDescriptorPool(VkDevice device, VkDescriptorPool descriptorPool,
                        const VkAllocationCallbacks *pAllocator)
{
  if (descriptorPool)
  {
    for (auto DS : descriptorPool->Pool)
      destroyDescriptorSet(device, DS);
    delete descriptorPool;
  }
}

VKAPI_ATTR void VKAPI_CALL vkDestroyDescriptorSetLayout(
    VkDevice device, VkDescriptorSetLayout descriptorSetLayout,
    const VkAllocationCallbacks *pAllocator)
{
  delete descriptorSetLayout;
}

VKAPI_ATTR void VKAPI_CALL vkDestroyDescriptorUpdateTemplate(
    VkDevice device, VkDescriptorUpdateTemplate descriptorUpdateTemplate,
    const VkAllocationCallbacks *pAllocator)
{
  if (descriptorUpdateTemplate)
  {
    delete[] descriptorUpdateTemplate->Entries;
    delete descriptorUpdateTemplate;
  }
}

VKAPI_ATTR void VKAPI_CALL vkDestroyDescriptorUpdateTemplateKHR(
    VkDevice device, VkDescriptorUpdateTemplate descriptorUpdateTemplate,
    const VkAllocationCallbacks *pAllocator)
{
  vkDestroyDescriptorUpdateTemplate(device, descriptorUpdateTemplate,
                                    pAllocator);
}

VKAPI_ATTR void VKAPI_CALL
vkDestroyPipelineLayout(VkDevice device, VkPipelineLayout pipelineLayout,
                        const VkAllocationCallbacks *pAllocator)
{
  delete pipelineLayout;
}

VKAPI_ATTR VkResult VKAPI_CALL vkFreeDescriptorSets(
    VkDevice device, VkDescriptorPool descriptorPool,
    uint32_t descriptorSetCount, const VkDescriptorSet *pDescriptorSets)
{
  for (uint32_t i = 0; i < descriptorSetCount; i++)
  {
    descriptorPool->Pool.erase(pDescriptorSets[i]);
    destroyDescriptorSet(device, pDescriptorSets[i]);
  }
  return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL vkGetDescriptorSetLayoutSupport(
    VkDevice device, const VkDescriptorSetLayoutCreateInfo *pCreateInfo,
    VkDescriptorSetLayoutSupport *pSupport)
{
  TALVOS_ABORT_UNIMPLEMENTED;
}

VKAPI_ATTR void VKAPI_CALL vkGetDescriptorSetLayoutSupportKHR(
    VkDevice device, const VkDescriptorSetLayoutCreateInfo *pCreateInfo,
    VkDescriptorSetLayoutSupport *pSupport)
{
  vkGetDescriptorSetLayoutSupport(device, pCreateInfo, pSupport);
}

VKAPI_ATTR VkResult VKAPI_CALL
vkResetDescriptorPool(VkDevice device, VkDescriptorPool descriptorPool,
                      VkDescriptorPoolResetFlags flags)
{
  for (auto &DS : descriptorPool->Pool)
    destroyDescriptorSet(device, DS);

  descriptorPool->Pool.clear();

  return VK_SUCCESS;
}

// Helper to update the descriptors for a particular descriptor set.
// The Get* parameters are lambdas that return pointers to the descriptor info
// structures at a particular binding.
template <typename BufFunc, typename ImgFunc, typename TexBufFunc>
void updateDescriptors(VkDevice Device, VkDescriptorSet Set,
                       VkDescriptorType Type, uint32_t Binding,
                       uint32_t ArrayElement, uint32_t Count,
                       const BufFunc &GetBufferDescriptorInfo,
                       const ImgFunc &GetImageDescriptorInfo,
                       const TexBufFunc &GetTexelBuffer)
{
  for (uint32_t b = 0; b < Count; b++)
  {
    // Check if the current binding is complete.
    assert(Set->Layout->BindingCounts.count(Binding));
    if (ArrayElement >= Set->Layout->BindingCounts.at(Binding))
    {
      ArrayElement = 0;

      // Increment binding, skipping any that have a descriptor count of 0.
      while (Set->Layout->BindingCounts.at(++Binding) == 0)
        ;
    }

    // Get the address of the resource.
    uint64_t Address;
    uint64_t NumBytes;
    switch (Type)
    {
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
    {
      const VkDescriptorBufferInfo *BufferInfo =
          (const VkDescriptorBufferInfo *)GetBufferDescriptorInfo(b);
      Address = BufferInfo->buffer->Address + BufferInfo->offset;
      NumBytes = BufferInfo->range;
      if (NumBytes == VK_WHOLE_SIZE)
        NumBytes = BufferInfo->buffer->NumBytes - BufferInfo->offset;
      break;
    }
    case VK_DESCRIPTOR_TYPE_SAMPLER:
    {
      const VkDescriptorImageInfo *ImageInfo =
          (const VkDescriptorImageInfo *)GetImageDescriptorInfo(b);
      Address = ImageInfo->sampler->ObjectAddress;
      NumBytes = sizeof(talvos::Sampler *);
      break;
    }
    case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
    {
      const VkDescriptorImageInfo *ImageInfo =
          (const VkDescriptorImageInfo *)GetImageDescriptorInfo(b);
      Address = ImageInfo->imageView->ObjectAddress;
      NumBytes = sizeof(talvos::ImageView *);
      break;
    }
    case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
    {
      const VkDescriptorImageInfo *ImageInfo =
          (const VkDescriptorImageInfo *)GetImageDescriptorInfo(b);

      // Allocate memory to store SampledImage object if necessary.
      talvos::Memory &Mem = Device->Device->getGlobalMemory();
      if (Set->CombinedImageSamplers.count({Binding, ArrayElement}))
        Address = Set->CombinedImageSamplers[{Binding, ArrayElement}];
      else
        Address = Mem.allocate(sizeof(talvos::SampledImage));

      // Create SampledImage object and store to global memory.
      talvos::SampledImage SI;
      SI.Image = ImageInfo->imageView->ImageView;
      if (Set->Layout->ImmutableSamplers.count(Binding))
        SI.Sampler =
            Set->Layout->ImmutableSamplers[Binding][ArrayElement]->Sampler;
      else
        SI.Sampler = ImageInfo->sampler->Sampler;
      Mem.store(Address, sizeof(talvos::SampledImage), (uint8_t *)&SI);
      Set->CombinedImageSamplers[{Binding, ArrayElement}] = Address;

      NumBytes = sizeof(talvos::SampledImage);

      break;
    }
    case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
    case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
    {
      const VkBufferView *TexelBuffer = (const VkBufferView *)GetTexelBuffer(b);
      Address = (*TexelBuffer)->ObjectAddress;
      NumBytes = sizeof(talvos::ImageView *);
      break;
    }
    default:
      assert(false && "unhandled descriptor type");
      abort();
    }

    // Set address for target binding and array element.
    Set->DescriptorSet[{Binding, ArrayElement}] = {Address, NumBytes};

    ++ArrayElement;
  }
}

VKAPI_ATTR void VKAPI_CALL vkUpdateDescriptorSetWithTemplate(
    VkDevice device, VkDescriptorSet descriptorSet,
    VkDescriptorUpdateTemplate descriptorUpdateTemplate, const void *pData)
{
  for (uint32_t i = 0; i < descriptorUpdateTemplate->EntryCount; i++)
  {
    const VkDescriptorUpdateTemplateEntry &Entry =
        descriptorUpdateTemplate->Entries[i];

    // Return a pointer to the descriptor info for a specific binding.
    auto GetDescriptorInfo = [pData, Entry](uint32_t Binding) -> const void * {
      return ((uint8_t *)pData) + Entry.offset + Entry.stride * Binding;
    };

    updateDescriptors(device, descriptorSet, Entry.descriptorType,
                      Entry.dstBinding, Entry.dstArrayElement,
                      Entry.descriptorCount, GetDescriptorInfo,
                      GetDescriptorInfo, GetDescriptorInfo);
  }
}

VKAPI_ATTR void VKAPI_CALL vkUpdateDescriptorSetWithTemplateKHR(
    VkDevice device, VkDescriptorSet descriptorSet,
    VkDescriptorUpdateTemplate descriptorUpdateTemplate, const void *pData)
{
  vkUpdateDescriptorSetWithTemplate(device, descriptorSet,
                                    descriptorUpdateTemplate, pData);
}

VKAPI_ATTR void VKAPI_CALL vkUpdateDescriptorSets(
    VkDevice device, uint32_t descriptorWriteCount,
    const VkWriteDescriptorSet *pDescriptorWrites, uint32_t descriptorCopyCount,
    const VkCopyDescriptorSet *pDescriptorCopies)
{
  // TODO: Handle copies.
  assert(descriptorCopyCount == 0 && "descriptor set copies not implemented");

  for (uint32_t i = 0; i < descriptorWriteCount; i++)
  {
    // Lambdas to return pointers to the descriptor info for a specific binding.
    VkWriteDescriptorSet Write = pDescriptorWrites[i];
    auto GetBufferDescriptorInfo = [Write](uint32_t Binding) -> const void * {
      return &(Write.pBufferInfo[Binding]);
    };
    auto GetImageDescriptorInfo = [Write](uint32_t Binding) -> const void * {
      return &(Write.pImageInfo[Binding]);
    };
    auto GetTexelBufferView = [Write](uint32_t Binding) -> const void * {
      return &(Write.pTexelBufferView[Binding]);
    };

    updateDescriptors(
        device, pDescriptorWrites[i].dstSet,
        pDescriptorWrites[i].descriptorType, pDescriptorWrites[i].dstBinding,
        pDescriptorWrites[i].dstArrayElement,
        pDescriptorWrites[i].descriptorCount, GetBufferDescriptorInfo,
        GetImageDescriptorInfo, GetTexelBufferView);
  }
}
