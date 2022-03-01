// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/rules/basic_event.h>

namespace nx::vms::rules {

class NX_VMS_RULES_API ServerConflictEvent: public BasicEvent
{
    Q_OBJECT
    Q_CLASSINFO("type", "nx.events.serverConflict")

public:
    static FilterManifest filterManifest();
    static const ItemDescriptor& manifest();
};

} // namespace nx::vms::rules
