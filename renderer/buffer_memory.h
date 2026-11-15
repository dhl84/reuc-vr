#pragma once
#include <vulkan/vulkan.h>

namespace uc {
// prefer_cached: a buffer that the CPU reads (a read-back). The first host-visible type of a desktop GPU is often write-combined memory
// without VK_MEMORY_PROPERTY_HOST_CACHED_BIT, and each read of it goes to the memory bus; a cached type is tried first.
template<class Allocate>
VkResult allocate_host_buffer(const VkPhysicalDeviceMemoryProperties& memory, uint32_t mask,
                              bool prefer_device_local, Allocate allocate, bool prefer_cached=false) {
    constexpr auto host=VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    VkResult result=VK_ERROR_FEATURE_NOT_PRESENT;
    for(unsigned pass=prefer_cached ? 0 : 1;pass<3;++pass) {
        const VkMemoryPropertyFlags required=host|(pass==0 ? VK_MEMORY_PROPERTY_HOST_CACHED_BIT : 0)|((prefer_device_local && pass==1) ? VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT : 0);
        for(uint32_t i=0;i<memory.memoryTypeCount;++i) {
            if(!(mask&(1u<<i)) || (memory.memoryTypes[i].propertyFlags&required)!=required) continue;
            mask&=~(1u<<i);
            result=allocate(i);
            if(result!=VK_ERROR_OUT_OF_DEVICE_MEMORY && result!=VK_ERROR_OUT_OF_HOST_MEMORY) return result;
        }
    }
    return result;
}
}
