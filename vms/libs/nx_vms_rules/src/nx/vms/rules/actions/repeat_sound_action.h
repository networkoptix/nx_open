// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../basic_action.h"
#include "../data_macros.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API RepeatSoundAction: public nx::vms::rules::BasicAction
{
    Q_OBJECT
    Q_CLASSINFO("type", "nx.actions.repeatSound")

    FIELD(QnUuidList, deviceIds, setDeviceIds)
    // TODO: #amalov Consider adding duration field or merging with PlaySound.
    FIELD(nx::vms::rules::UuidSelection, users, setUsers)
    FIELD(QString, sound, setSound)
    FIELD(float, volume, setVolume)

public:
    static const ItemDescriptor& manifest();
};

} // namespace nx::vms::rules
