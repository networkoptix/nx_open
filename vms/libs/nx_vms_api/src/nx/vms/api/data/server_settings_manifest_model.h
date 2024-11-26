// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/settings.h>

namespace nx::utils {

NX_REFLECTION_INSTRUMENT(SettingInfo, SettingInfo_Fields)
QN_FUSION_DECLARE_FUNCTIONS(SettingInfo, (json), NX_VMS_API)

} // nx::utils

namespace nx::vms::api {

struct SettingsDescription: public std::map<QString, nx::utils::SettingInfo>
{
};

using ServerSettingsManifestValues = std::map<nx::Uuid, SettingsDescription>;

} // nx::vms::api
