// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "notification_action_base.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API RepeatSoundAction: public NotificationActionBase
{
    Q_OBJECT
    Q_CLASSINFO("type", "nx.actions.repeatSound")

    // TODO: #amalov Consider adding duration field or merging with PlaySound.
    FIELD(QString, sound, setSound)
    FIELD(float, volume, setVolume)

public:
    RepeatSoundAction();

    static const ItemDescriptor& manifest();
};

} // namespace nx::vms::rules
