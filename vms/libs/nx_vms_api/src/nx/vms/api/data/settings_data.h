// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>

#include <QtCore/QJsonValue>

#include <nx/reflect/instrument.h>

#include "data_macros.h"
#include "server_settings_manifest_model.h"

namespace nx::vms::api {

NX_REFLECTION_ENUM_CLASS(LocationType,
    /**%apidoc Config information is located on the filesystem. */
    fileSystem,

    /**%apidoc Config information is located in the registry. */
    registry
)

struct NX_VMS_API SettingsInfo
{
    nx::Uuid serverId;
    SettingsDescription settingsDescription;
    LocationType type{LocationType::fileSystem};
    std::string location;
};
#define SettingsInfo_Fields (serverId)(settingsDescription)(type)(location)
NX_VMS_API_DECLARE_STRUCT_AND_LIST_EX(SettingsInfo, (json))
NX_REFLECTION_INSTRUMENT(SettingsInfo, SettingsInfo_Fields)

} // namespace nx::vms::api
