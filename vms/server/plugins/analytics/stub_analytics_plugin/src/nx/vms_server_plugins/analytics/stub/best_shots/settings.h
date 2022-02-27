// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace best_shots {

extern const std::string kBestShotGenerationPolicySetting;
extern const std::string kFixedBoundingBoxBestShotGenerationPolicy;
extern const std::string kUrlBestShotGenerationPolicy;
extern const std::string kImageBestShotGenerationPolicy;

extern const std::string kObjectCountSetting;
extern const std::string kFrameNumberToGenerateBestShotSetting;

extern const std::string kTopLeftXSetting;
extern const std::string kTopLeftYSetting;
extern const std::string kWidthSetting;
extern const std::string kHeightSetting;

extern const std::string kUrlSetting;
extern const std::string kImagePathSetting;

} // namespace best_shots
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
