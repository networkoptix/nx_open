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

extern const std::string kBestShotObjectCountSetting;
extern const std::string kFrameNumberToGenerateBestShotSetting;

extern const std::string kBestShotTopLeftXSetting;
extern const std::string kBestShotTopLeftYSetting;
extern const std::string kBestShotWidthSetting;
extern const std::string kBestShotHeightSetting;

extern const std::string kBestShotUrlSetting;
extern const std::string kBestShotImagePathSetting;

extern const std::string kTitleGenerationPolicySetting;
extern const std::string kUrlTitleGenerationPolicy;
extern const std::string kImageTitleGenerationPolicy;
extern const std::string kFixedBoundingBoxTitleGenerationPolicy;
extern const std::string kTitleUrlSetting;
extern const std::string kTitleImagePathSetting;
extern const std::string kTitleTextSetting;

extern const std::string kTitleTopLeftXSetting;
extern const std::string kTitleTopLeftYSetting;
extern const std::string kTitleWidthSetting;
extern const std::string kTitleHeightSetting;

extern const std::string kTitleObjectCountSetting;
extern const std::string kFrameNumberToGenerateTitleSetting;

} // namespace best_shots
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
