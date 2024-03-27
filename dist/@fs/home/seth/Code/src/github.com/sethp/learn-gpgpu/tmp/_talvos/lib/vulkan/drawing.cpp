// Copyright (c) 2018 the Talvos developers. All rights reserved.
//
// This file is distributed under a three-clause BSD license. For full license
// terms please see the LICENSE file distributed with this source code.

#include "runtime.h"

#include "talvos/Commands.h"

VKAPI_ATTR void VKAPI_CALL vkCmdBindIndexBuffer(VkCommandBuffer commandBuffer,
                                                VkBuffer buffer,
                                                VkDeviceSize offset,
                                                VkIndexType indexType)
{
  commandBuffer->IndexBufferAddress = buffer->Address + offset;
  commandBuffer->IndexType = indexType;
}

VKAPI_ATTR void VKAPI_CALL vkCmdDraw(VkCommandBuffer commandBuffer,
                                     uint32_t vertexCount,
                                     uint32_t instanceCount,
                                     uint32_t firstVertex,
                                     uint32_t firstInstance)
{
  commandBuffer->Commands.push_back(new talvos::DrawCommand(
      commandBuffer->PipelineContext, commandBuffer->RenderPassInstance,
      vertexCount, firstVertex, instanceCount, firstInstance));
}

VKAPI_ATTR void VKAPI_CALL vkCmdDrawIndexed(
    VkCommandBuffer commandBuffer, uint32_t indexCount, uint32_t instanceCount,
    uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance)
{
  commandBuffer->Commands.push_back(new talvos::DrawIndexedCommand(
      commandBuffer->PipelineContext, commandBuffer->RenderPassInstance,
      indexCount, firstIndex, vertexOffset, instanceCount, firstInstance,
      commandBuffer->IndexBufferAddress, commandBuffer->IndexType));
}

VKAPI_ATTR void VKAPI_CALL vkCmdDrawIndexedIndirect(
    VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset,
    uint32_t drawCount, uint32_t stride)
{
  TALVOS_ABORT_UNIMPLEMENTED;
}

VKAPI_ATTR void VKAPI_CALL vkCmdDrawIndirect(VkCommandBuffer commandBuffer,
                                             VkBuffer buffer,
                                             VkDeviceSize offset,
                                             uint32_t drawCount,
                                             uint32_t stride)
{
  TALVOS_ABORT_UNIMPLEMENTED;
}
