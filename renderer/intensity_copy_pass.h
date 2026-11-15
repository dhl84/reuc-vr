#pragma once
#include <vulkan/vulkan.h>
#include <filesystem>

namespace uc {
// The colour conversion of an EFB copy to an intensity format (GX_TF_I8): every channel of the copy becomes
// Y = 16/255 + 0.257 R + 0.504 G + 0.098 B, as the hardware writes it. The pass works in place on one layer of an RGBA8 image
// in VK_IMAGE_LAYOUT_GENERAL. Only a game that makes such copies creates it (SBDE08).
class IntensityCopyPass {
public:
    void initialize(VkDevice device, const std::filesystem::path& shaders);
    bool ready() const { return pipeline_ != VK_NULL_HANDLE; }
    // One set for each image view (a 2D view of one layer, storage usage). The caller frees the set before it destroys the view.
    VkDescriptorSet make_set(VkImageView view);
    void free_set(VkDescriptorSet set);
    void record(VkCommandBuffer command, VkDescriptorSet set, VkExtent2D extent);
    void shutdown();
private:
    VkDevice device_=VK_NULL_HANDLE;
    VkShaderModule module_=VK_NULL_HANDLE;
    VkDescriptorSetLayout set_layout_=VK_NULL_HANDLE;
    VkPipelineLayout layout_=VK_NULL_HANDLE;
    VkPipeline pipeline_=VK_NULL_HANDLE;
    VkDescriptorPool pool_=VK_NULL_HANDLE;
};
}
