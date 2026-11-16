#include "tev_fold.h"
#ifdef UC_DEVICE_FOLD
#include "spirv-tools/optimizer.hpp"
#include <unordered_map>
#endif
namespace uc {
#ifdef UC_DEVICE_FOLD
bool tev_fold_available() { return true; }
std::vector<uint32_t> tev_fold(const std::vector<uint32_t>& base,const std::array<uint32_t,68>& values,std::string& error) {
    spvtools::Optimizer optimizer(SPV_ENV_UNIVERSAL_1_6);
    optimizer.SetMessageConsumer([&error](spv_message_level_t level,const char*,const spv_position_t&,const char* message) {
        if (level<=SPV_MSG_ERROR && error.empty() && message) error=message;
    });
    // spirv-opt --set-spec-const-default-value "0:v0 1:v1 ...", then --freeze-spec-const --fold-spec-const-op-composite -O
    std::unordered_map<uint32_t,std::string> defaults;
    for (uint32_t id=0;id<values.size();++id) defaults.emplace(id,std::to_string(values[id]));
    optimizer.RegisterPass(spvtools::CreateSetSpecConstantDefaultValuePass(defaults));
    optimizer.RegisterPass(spvtools::CreateFreezeSpecConstantValuePass());
    optimizer.RegisterPass(spvtools::CreateFoldSpecConstantOpAndCompositePass());
    optimizer.RegisterPerformancePasses();
    spvtools::OptimizerOptions options;
    options.set_run_validator(false);   // the module is the build's own
    std::vector<uint32_t> folded;
    if (!optimizer.Run(base.data(),base.size(),&folded,options) || folded.empty()) {
        if (error.empty()) error="the optimizer gave no module";
        folded.clear();
    }
    return folded;
}
#else
bool tev_fold_available() { return false; }
std::vector<uint32_t> tev_fold(const std::vector<uint32_t>&,const std::array<uint32_t,68>&,std::string& error) {
    error="this build has no optimizer (UC_DEVICE_FOLD)";
    return {};
}
#endif
}
