#include "intensity_copy_pass.h"
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace uc {
namespace {
void check(VkResult result, const char* what) { if (result != VK_SUCCESS) throw std::runtime_error(std::string("intensity copy: ") + what + " failed: VkResult " + std::to_string(result)); }
struct Constants { int32_t width, height; };
constexpr uint32_t kSets = 256;   // live intensity copies times layers; a frame of SBDE08 holds about ten
}
void IntensityCopyPass::initialize(VkDevice device, const std::filesystem::path& shaders) {
    if (pipeline_) return;
    device_ = device;
    try {
        std::ifstream file(shaders / "intensity_copy.comp.spv", std::ios::binary);
        std::vector<char> code((std::istreambuf_iterator<char>(file)), {});
        if (code.empty() || code.size() % 4) throw std::runtime_error("intensity copy: shader intensity_copy.comp.spv missing or invalid");
        VkShaderModuleCreateInfo module{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO}; module.codeSize = code.size(); module.pCode = reinterpret_cast<const uint32_t*>(code.data());
        check(vkCreateShaderModule(device_, &module, nullptr, &module_), "shader module");
        const VkDescriptorSetLayoutBinding binding{0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
        VkDescriptorSetLayoutCreateInfo set{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO}; set.bindingCount = 1; set.pBindings = &binding;
        check(vkCreateDescriptorSetLayout(device_, &set, nullptr, &set_layout_), "descriptor layout");
        const VkPushConstantRange push{VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(Constants)};
        VkPipelineLayoutCreateInfo layout{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO}; layout.setLayoutCount = 1; layout.pSetLayouts = &set_layout_; layout.pushConstantRangeCount = 1; layout.pPushConstantRanges = &push;
        check(vkCreatePipelineLayout(device_, &layout, nullptr, &layout_), "pipeline layout");
        const VkDescriptorPoolSize size{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, kSets};
        VkDescriptorPoolCreateInfo pool{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO}; pool.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT; pool.maxSets = kSets; pool.poolSizeCount = 1; pool.pPoolSizes = &size;
        check(vkCreateDescriptorPool(device_, &pool, nullptr, &pool_), "descriptor pool");
        VkComputePipelineCreateInfo pipeline{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
        pipeline.stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_COMPUTE_BIT, module_, "main", nullptr};
        pipeline.layout = layout_;
        check(vkCreateComputePipelines(device_, VK_NULL_HANDLE, 1, &pipeline, nullptr, &pipeline_), "pipeline");   // last: ready() tells a complete pass
    } catch (...) { shutdown(); throw; }
}
VkDescriptorSet IntensityCopyPass::make_set(VkImageView view) {
    VkDescriptorSet result = VK_NULL_HANDLE;
    VkDescriptorSetAllocateInfo allocate{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO}; allocate.descriptorPool = pool_; allocate.descriptorSetCount = 1; allocate.pSetLayouts = &set_layout_;
    check(vkAllocateDescriptorSets(device_, &allocate, &result), "descriptor set");
    const VkDescriptorImageInfo target{VK_NULL_HANDLE, view, VK_IMAGE_LAYOUT_GENERAL};
    VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}; write.dstSet = result; write.descriptorCount = 1; write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE; write.pImageInfo = &target;
    vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);
    return result;
}
void IntensityCopyPass::free_set(VkDescriptorSet set) {
    if (set && pool_) vkFreeDescriptorSets(device_, pool_, 1, &set);
}
void IntensityCopyPass::record(VkCommandBuffer command, VkDescriptorSet set, VkExtent2D extent) {
    vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_);
    vkCmdBindDescriptorSets(command, VK_PIPELINE_BIND_POINT_COMPUTE, layout_, 0, 1, &set, 0, nullptr);
    const Constants constants{int32_t(extent.width), int32_t(extent.height)};
    vkCmdPushConstants(command, layout_, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof constants, &constants);
    vkCmdDispatch(command, (extent.width + 7) / 8, (extent.height + 7) / 8, 1);
}
void IntensityCopyPass::shutdown() {
    if (!device_) return;
    if (pool_) vkDestroyDescriptorPool(device_, pool_, nullptr);
    if (pipeline_) vkDestroyPipeline(device_, pipeline_, nullptr);
    if (layout_) vkDestroyPipelineLayout(device_, layout_, nullptr);
    if (set_layout_) vkDestroyDescriptorSetLayout(device_, set_layout_, nullptr);
    if (module_) vkDestroyShaderModule(device_, module_, nullptr);
    *this = IntensityCopyPass{};
}
}
