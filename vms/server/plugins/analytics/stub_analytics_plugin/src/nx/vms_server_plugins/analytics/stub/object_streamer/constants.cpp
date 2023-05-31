// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "constants.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace object_streamer {

const std::string kAutoTrackIdPerDeviceAgentCreationPrefix = "$";
const std::string kAutoTrackIdPerStreamCyclePrefix = "$$";

const std::string kTypeIdField = "typeId";
const std::string kTrackIdField = "trackId";
const std::string kFrameNumberField = "frameNumber";
const std::string kAttributesField = "attributes";
const std::string kBoundingBoxField = "boundingBox";
const std::string kTopLeftXField = "x";
const std::string kTopLeftYField = "y";
const std::string kWidthField = "width";
const std::string kHeightField = "height";
const std::string kTimestampUsField = "timestampUs";
const std::string kEntryTypeField = "entryType";
const std::string kImageSourceField = "imageSource";

const std::string kRegularEntryType = "regular";
const std::string kBestShotEntryType = "bestShot";

const std::string kDefaultManifestFile = "object_streamer/manifest.json";
const std::string kDefaultStreamFile = "object_streamer/stream.json";

const std::string kManifestFileSetting = "manifestFile";
const std::string kStreamFileSetting = "streamFile";

const std::string kObjectTypeFilterPrefix = "object_type_filter_";

} // namespace object_streamer
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
