// Copyright (c) 2018 the Talvos developers. All rights reserved.
//
// This file is distributed under a three-clause BSD license. For full license
// terms please see the LICENSE file distributed with this source code.

#include "runtime.h"

#include "talvos/Image.h"
#include "talvos/RenderPass.h"

VKAPI_ATTR void VKAPI_CALL vkCmdBeginRenderPass(
    VkCommandBuffer commandBuffer,
    const VkRenderPassBeginInfo *pRenderPassBegin, VkSubpassContents contents)
{
  assert(pRenderPassBegin->renderArea.offset.x == 0 &&
         pRenderPassBegin->renderArea.offset.y == 0 &&
         pRenderPassBegin->renderArea.extent.width ==
             pRenderPassBegin->framebuffer->Framebuffer->getWidth() &&
         pRenderPassBegin->renderArea.extent.height ==
             pRenderPassBegin->framebuffer->Framebuffer->getHeight() &&
         "using render area smaller than framebuffer not implemented");

  std::vector<VkClearValue> ClearValues(pRenderPassBegin->pClearValues,
                                        pRenderPassBegin->pClearValues +
                                            pRenderPassBegin->clearValueCount);

  // Create the render pass instance object.
  commandBuffer->RenderPassInstance =
      std::make_shared<talvos::RenderPassInstance>(
          *pRenderPassBegin->renderPass->RenderPass,
          *pRenderPassBegin->framebuffer->Framebuffer, ClearValues);

  // Create the render pass begin command.
  commandBuffer->Commands.push_back(
      new talvos::BeginRenderPassCommand(commandBuffer->RenderPassInstance));
}

VKAPI_ATTR void VKAPI_CALL vkCmdEndRenderPass(VkCommandBuffer commandBuffer)
{
  commandBuffer->Commands.push_back(
      new talvos::EndRenderPassCommand(commandBuffer->RenderPassInstance));

  commandBuffer->RenderPassInstance.reset();
}

VKAPI_ATTR void VKAPI_CALL vkCmdNextSubpass(VkCommandBuffer commandBuffer,
                                            VkSubpassContents contents)
{
  commandBuffer->Commands.push_back(
      new talvos::NextSubpassCommand(commandBuffer->RenderPassInstance));
}

VKAPI_ATTR VkResult VKAPI_CALL vkCreateFramebuffer(
    VkDevice device, const VkFramebufferCreateInfo *pCreateInfo,
    const VkAllocationCallbacks *pAllocator, VkFramebuffer *pFramebuffer)
{
  // Build list of attachments.
  std::vector<talvos::ImageView *> Attachments;
  for (uint32_t i = 0; i < pCreateInfo->attachmentCount; i++)
    Attachments.push_back(pCreateInfo->pAttachments[i]->ImageView);

  // Create framebuffer.
  *pFramebuffer = new VkFramebuffer_T;
  (*pFramebuffer)->Framebuffer = new talvos::Framebuffer(
      *device->Device, pCreateInfo->width, pCreateInfo->height,
      pCreateInfo->layers, Attachments);

  return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL vkCreateRenderPass(
    VkDevice device, const VkRenderPassCreateInfo *pCreateInfo,
    const VkAllocationCallbacks *pAllocator, VkRenderPass *pRenderPass)
{
  assert(pCreateInfo->dependencyCount == 0 &&
         "subpass dependencies not implemented");

  // Prepare attachment descriptions.
  std::vector<VkAttachmentDescription> Attachments(
      pCreateInfo->pAttachments,
      pCreateInfo->pAttachments + pCreateInfo->attachmentCount);

  // Build subpass structures.
  std::vector<talvos::Subpass> Subpasses(pCreateInfo->subpassCount);
  for (uint32_t i = 0; i < pCreateInfo->subpassCount; i++)
  {
    const VkSubpassDescription &Desc = pCreateInfo->pSubpasses[i];

    for (uint32_t a = 0; a < Desc.inputAttachmentCount; a++)
      Subpasses[i].InputAttachments.push_back(
          Desc.pInputAttachments[a].attachment);

    for (uint32_t a = 0; a < Desc.colorAttachmentCount; a++)
    {
      if (Desc.pColorAttachments)
        Subpasses[i].ColorAttachments.push_back(
            Desc.pColorAttachments[a].attachment);
      if (Desc.pResolveAttachments)
        Subpasses[i].ResolveAttachments.push_back(
            Desc.pResolveAttachments[a].attachment);
    }

    for (uint32_t a = 0; a < Desc.preserveAttachmentCount; a++)
      Subpasses[i].PreserveAttachments.push_back(Desc.pPreserveAttachments[a]);

    if (Desc.pDepthStencilAttachment)
      Subpasses[i].DepthStencilAttachment =
          Desc.pDepthStencilAttachment->attachment;
  }

  // Create render pass object.
  *pRenderPass = new VkRenderPass_T;
  (*pRenderPass)->RenderPass = new talvos::RenderPass(Attachments, Subpasses);

  return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL
vkDestroyFramebuffer(VkDevice device, VkFramebuffer framebuffer,
                     const VkAllocationCallbacks *pAllocator)
{
  if (framebuffer)
  {
    delete framebuffer->Framebuffer;
    delete framebuffer;
  }
}

VKAPI_ATTR void VKAPI_CALL
vkDestroyRenderPass(VkDevice device, VkRenderPass renderPass,
                    const VkAllocationCallbacks *pAllocator)
{
  if (renderPass)
  {
    delete renderPass->RenderPass;
    delete renderPass;
  }
}

VKAPI_ATTR void VKAPI_CALL vkGetRenderAreaGranularity(VkDevice device,
                                                      VkRenderPass renderPass,
                                                      VkExtent2D *pGranularity)
{
  pGranularity->width = 1;
  pGranularity->height = 1;
}
