// Copyright (c) 2018 the Talvos developers. All rights reserved.
//
// This file is distributed under a three-clause BSD license. For full license
// terms please see the LICENSE file distributed with this source code.

#include "runtime.h"

#include <algorithm>
#include <cstring>

// List of supported instance extensions.
const VkExtensionProperties InstanceExtensions[] = {
    // TODO this breaks the vulkan loader somehow
    // {VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
    //  VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_SPEC_VERSION},
};
const uint32_t NumInstanceExtensions =
    sizeof(InstanceExtensions) / sizeof(VkExtensionProperties);

// List of supported device extensions.
const VkExtensionProperties DeviceExtensions[] = {
    {VK_KHR_8BIT_STORAGE_EXTENSION_NAME, VK_KHR_8BIT_STORAGE_SPEC_VERSION},
    {VK_KHR_BIND_MEMORY_2_EXTENSION_NAME, VK_KHR_BIND_MEMORY_2_SPEC_VERSION},
    {VK_KHR_STORAGE_BUFFER_STORAGE_CLASS_EXTENSION_NAME,
     VK_KHR_STORAGE_BUFFER_STORAGE_CLASS_SPEC_VERSION},
    {VK_KHR_VARIABLE_POINTERS_EXTENSION_NAME,
     VK_KHR_VARIABLE_POINTERS_SPEC_VERSION},
};
const uint32_t NumDeviceExtensions =
    sizeof(DeviceExtensions) / sizeof(VkExtensionProperties);

VKAPI_ATTR VkResult VKAPI_CALL vkEnumerateDeviceExtensionProperties(
    VkPhysicalDevice physicalDevice, const char *pLayerName,
    uint32_t *pPropertyCount, VkExtensionProperties *pProperties)
{
  if (!pProperties)
  {
    *pPropertyCount = NumDeviceExtensions;
    return VK_SUCCESS;
  }

  // Copy extension properties to output.
  uint32_t Count = std::min(NumDeviceExtensions, *pPropertyCount);
  memcpy(pProperties, DeviceExtensions, Count * sizeof(VkExtensionProperties));

  if (Count < NumDeviceExtensions)
    return VK_INCOMPLETE;
  else
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL vkEnumerateDeviceLayerProperties(
    VkPhysicalDevice physicalDevice, uint32_t *pPropertyCount,
    VkLayerProperties *pProperties)
{
  *pPropertyCount = 0;
  return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL vkEnumerateInstanceExtensionProperties(
    const char *pLayerName, uint32_t *pPropertyCount,
    VkExtensionProperties *pProperties)
{
  if (!pProperties)
  {
    *pPropertyCount = NumInstanceExtensions;
    return VK_SUCCESS;
  }

  // Copy extension properties to output.
  uint32_t Count = std::min(NumInstanceExtensions, *pPropertyCount);
  memcpy(pProperties, InstanceExtensions,
         Count * sizeof(VkExtensionProperties));

  if (Count < NumInstanceExtensions)
    return VK_INCOMPLETE;
  else
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL vkEnumerateInstanceLayerProperties(
    uint32_t *pPropertyCount, VkLayerProperties *pProperties)
{
  *pPropertyCount = 0;
  return VK_SUCCESS;
}
