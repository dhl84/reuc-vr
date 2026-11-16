#pragma once
// The headset: the TEV fragment module specialized for one pipeline key at run time. tools/fold_tev_shaders.py does the same at build
// time for the keys of the build's lists (pipeline_keys.seed, pipeline_keys.quest); this is for a key that no list has. The passes are
// those of the tool: the key's constants as the defaults of the specialization constants, the constants frozen, the constant
// operations folded, then the performance passes. The work is the optimizer of SPIRV-Tools (CMake option UC_DEVICE_FOLD).
#include <array>
#include <cstdint>
#include <string>
#include <vector>
namespace uc {
bool tev_fold_available();   // false in a build without UC_DEVICE_FOLD
// base: the words of the TEV fragment module. values: its 68 specialization constants (make_pipeline). Returns the words of the folded
// module, or nothing with the reason in error.
std::vector<uint32_t> tev_fold(const std::vector<uint32_t>& base,const std::array<uint32_t,68>& values,std::string& error);
}
