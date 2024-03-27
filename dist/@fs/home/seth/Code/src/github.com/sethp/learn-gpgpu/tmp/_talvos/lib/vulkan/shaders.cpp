// Copyright (c) 2018 the Talvos developers. All rights reserved.
//
// This file is distributed under a three-clause BSD license. For full license
// terms please see the LICENSE file distributed with this source code.

#include "runtime.h"

#include "talvos/Module.h"
#include <spirv-tools/libspirv.h>
#include <spirv-tools/libspirv.hpp>

VKAPI_ATTR VkResult VKAPI_CALL vkCreateShaderModule(
    VkDevice device, const VkShaderModuleCreateInfo *pCreateInfo,
    const VkAllocationCallbacks *pAllocator, VkShaderModule *pShaderModule)
{
  spvtools::Context SPVContext(SPV_ENV_VULKAN_1_3);
  *pShaderModule = new VkShaderModule_T;
  (*pShaderModule)->Module = talvos::Module::load(
      SPVContext, pCreateInfo->pCode, pCreateInfo->codeSize / 4);
  return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL
vkDestroyShaderModule(VkDevice device, VkShaderModule shaderModule,
                      const VkAllocationCallbacks *pAllocator)
{
  delete shaderModule;
}
