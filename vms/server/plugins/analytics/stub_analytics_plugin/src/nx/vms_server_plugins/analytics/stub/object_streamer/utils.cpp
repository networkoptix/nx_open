// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "utils.h"

#include <fstream>
#include <streambuf>

#include "../utils.h"
#include "constants.h"

#include <nx/kit/json.h>

using nx::kit::Json;

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace object_streamer {

std::string defaultManifestFilePath(const std::string& pluginHomeDir)
{
    return pluginHomeDir + "/" + kDefaultManifestFile;
}

std::string defaultStreamFilePath(const std::string& pluginHomeDir)
{
    return pluginHomeDir + "/" + kDefaultStreamFile;
}

std::string makeObjectTypeFilterSettingName(const std::string& objectTypeId)
{
    return kObjectTypeFilterPrefix + objectTypeId;
}

std::string readFileToString(const std::string& filePath)
{
    std::ifstream t(filePath);
    return std::string(std::istreambuf_iterator<char>(t), std::istreambuf_iterator<char>());
}

std::string makeSettingsModel(
    const std::string& manifestFilePath,
    const std::string& streamFilePath,
    const std::string& pluginHomeDir,
    const std::set<std::string>& objectTypeIds)
{
    Json::object manifestPathSetting;
    manifestPathSetting["type"] = "TextArea";
    manifestPathSetting["name"] = kManifestFileSetting;
    manifestPathSetting["caption"] = "Path to Device Agent Manifest file";
    manifestPathSetting["defaultValue"] = defaultManifestFilePath(pluginHomeDir);

    Json::object streamFilePathSetting;
    streamFilePathSetting["type"] = "TextArea";
    streamFilePathSetting["name"] = kStreamFileSetting;
    streamFilePathSetting["caption"] = "Path to Object stream file";
    streamFilePathSetting["defaultValue"] = defaultStreamFilePath(pluginHomeDir);

    Json::array pathSettingGroupItems = {manifestPathSetting, streamFilePathSetting};
    Json::object pathSettingGroup;
    pathSettingGroup["type"] = "GroupBox";
    pathSettingGroup["caption"] = "Paths";
    pathSettingGroup["items"] = std::move(pathSettingGroupItems);

    std::map<std::string, std::string> values;
    Json::array filterSettingGroupItems;
    for (const std::string& objectTypeId: objectTypeIds)
    {
        Json::object objectTypeFilter;
        objectTypeFilter["type"] = "CheckBox";
        objectTypeFilter["name"] = makeObjectTypeFilterSettingName(objectTypeId);
        objectTypeFilter["caption"] = objectTypeId;
        objectTypeFilter["defaultValue"] = true;

        filterSettingGroupItems.push_back(std::move(objectTypeFilter));
    }

    Json::object filterSettingGroup;
    filterSettingGroup["type"] = "GroupBox";
    filterSettingGroup["caption"] = "Filters";
    filterSettingGroup["items"] = std::move(filterSettingGroupItems);

    Json::object settingsModel;
    settingsModel["type"] = "Settings";
    settingsModel["items"] = std::vector<Json>{pathSettingGroup, filterSettingGroup};

    return Json(settingsModel).dump();
}

std::string makePluginDiagnosticEventDescription(const std::set<Issue>& issues)
{
    std::vector<std::string> issueStrings;
    for (const Issue issue: issues)
        issueStrings.push_back(issueToString(issue));

    return "The following issues have been found: " + join(issueStrings, ",\n");
}

} // namespace object_streamer
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
