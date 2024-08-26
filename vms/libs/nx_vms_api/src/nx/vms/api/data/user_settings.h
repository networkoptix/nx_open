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
     * Black list of events, contains events that are disabled.
     * %example ["nx.events.motion", "nx.events.deviceDisconnected"]
     */
    std::set<std::string> eventFilter;

    /**%apidoc[opt]
     * Preferred locale of the user. Will affect language of the notifications (and later emails)
     * displayed both in desktop and mobile client.
     * %example en_Us
     */
    std::string locale;

    bool operator==(const UserSettings& /*other*/) const = default;
};
#define UserSettings_Fields (eventFilter)(locale)
QN_FUSION_DECLARE_FUNCTIONS(UserSettings, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(UserSettings, UserSettings_Fields)

} // namespace nx::vms::api
