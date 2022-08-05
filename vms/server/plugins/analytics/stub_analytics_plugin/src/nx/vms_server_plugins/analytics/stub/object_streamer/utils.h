// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <set>
#include <string>

#include "stream_parser.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace object_streamer {

std::string defaultManifestFilePath(const std::string& pluginHomeDir);

std::string defaultStreamFilePath(const std::string& pluginHomeDir);

std::string makeObjectTypeFilterSettingName(const std::string& objectTypeId);

std::string readFileToString(const std::string& filePath);

std::string makeSettingsModel(
    const std::string& manifestFilePath,
    const std::string& streamFilePath,
    const std::string& pluginHomeDir,
    const std::set<std::string>& objectTypeIds);

std::string makePluginDiagnosticEventDescription(const std::set<Issue>& issues);

} // namespace object_streamer
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
