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

const std::string kObjectCountSetting = "objectCount";
const std::string kFrameNumberToGenerateBestShotSetting = "frameNumberToGenerateBestShot";

const std::string kTopLeftXSetting = "topLeftX";
const std::string kTopLeftYSetting = "topLeftY";
const std::string kWidthSetting = "width";
const std::string kHeightSetting = "height";

const std::string kUrlSetting = "url";
const std::string kImagePathSetting = "image";

} // namespace best_shots
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
