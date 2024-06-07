// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "settings.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace best_shots {

const std::string kBestShotGenerationPolicySetting = "bestShotGenerationPolicy";
const std::string kFixedBoundingBoxBestShotGenerationPolicy =
    "fixedBoundingBoxBestShotGenerationPolicy";
const std::string kUrlBestShotGenerationPolicy = "urlBestShotGenerationPolicy";
const std::string kImageBestShotGenerationPolicy = "imageBestShotGenerationPolicy";

const std::string kBestShotObjectCountSetting = "bestShotObjectCount";
const std::string kFrameNumberToGenerateBestShotSetting = "frameNumberToGenerateBestShot";

const std::string kBestShotTopLeftXSetting = "bestShotTopLeftX";
const std::string kBestShotTopLeftYSetting = "bestShotTopLeftY";
const std::string kBestShotWidthSetting = "bestShotWidth";
const std::string kBestShotHeightSetting = "bestShotHeight";

const std::string kBestShotUrlSetting = "bestShotUrl";
const std::string kBestShotImagePathSetting = "bestShotImage";

const std::string kTitleGenerationPolicySetting = "titleGenerationPolicy";
const std::string kUrlTitleGenerationPolicy = "urlTitleGenerationPolicy";
const std::string kImageTitleGenerationPolicy = "imageTitleGenerationPolicy";
const std::string kFixedBoundingBoxTitleGenerationPolicy =
    "fixedBoundingBoxTitleGenerationPolicy";

const std::string kTitleUrlSetting = "titleUrl";
const std::string kTitleImagePathSetting = "titleImage";
const std::string kTitleTextSetting = "titleText";

const std::string kTitleTopLeftXSetting = "titleTopLeftX";
const std::string kTitleTopLeftYSetting = "titleTopLeftY";
const std::string kTitleWidthSetting = "titleWidth";
const std::string kTitleHeightSetting = "titleHeight";

const std::string kTitleObjectCountSetting = "titleObjectCount";
const std::string kFrameNumberToGenerateTitleSetting = "frameNumberToGenerateTitle";

const std::string kEnableBestShotGeneration = "enableBestShotGeneration";
const std::string kEnableObjectTitleGeneration = "enableObjectTitleGeneration";

} // namespace best_shots
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
