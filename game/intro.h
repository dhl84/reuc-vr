#pragma once
#include "ppc_rt.h"
namespace uc { void intro_init(bool skip); }
void hle_IntroLogoUpdate(ppc::Ctx& c);
void hle_MovieUpdate(ppc::Ctx& c);
void hle_MTXPerspective(ppc::Ctx& c);
void hle_DcStagePartsViewTest(ppc::Ctx& c);   // SBDE08: 8032DCE4 (project/dc/hle_manual.txt)
