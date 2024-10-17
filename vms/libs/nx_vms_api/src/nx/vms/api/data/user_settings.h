// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <set>
#include <string>

#include <QtCore/QString>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/instrument.h>

namespace nx::vms::api {

struct NX_VMS_API UserSettings
{
    /**%apidoc[opt]:stringArray
     * Ban list of events, contains events that are disabled.
     * %example ["nx.events.motion", "nx.events.deviceDisconnected"]
     */
    std::set<std::string> eventFilter;

    /**%apidoc[opt]:stringArray
     * Ban list of messages, contains messages that are disabled.
     * %example ["emailIsEmpty", "noLicenses"]
     */
    std::set<std::string> messageFilter;

    bool operator==(const UserSettings& /*other*/) const = default;
};
#define UserSettings_Fields (eventFilter)(messageFilter)
QN_FUSION_DECLARE_FUNCTIONS(UserSettings, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(UserSettings, UserSettings_Fields)

} // namespace nx::vms::api
