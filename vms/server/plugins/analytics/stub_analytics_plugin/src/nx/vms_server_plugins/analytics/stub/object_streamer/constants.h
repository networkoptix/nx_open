// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace object_streamer {

extern const std::string kAutoTrackIdPerDeviceAgentCreationPrefix;
extern const std::string kAutoTrackIdPerStreamCyclePrefix;

extern const std::string kTypeIdField;
extern const std::string kTrackIdField;
extern const std::string kFrameNumberField;
extern const std::string kAttributesField;
extern const std::string kBoundingBoxField;
extern const std::string kTopLeftXField;
extern const std::string kTopLeftYField;
extern const std::string kWidthField;
extern const std::string kHeightField;
extern const std::string kTimestampUsField;
extern const std::string kEntryTypeField;
extern const std::string kImageSourceField;

extern const std::string kRegularEntryType;
extern const std::string kBestShotEntryType;

extern const std::string kDefaultManifestFile;
extern const std::string kDefaultStreamFile;

extern const std::string kManifestFileSetting;
extern const std::string kStreamFileSetting;

extern const std::string kObjectTypeFilterPrefix;

} // namespace object_streamer
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
