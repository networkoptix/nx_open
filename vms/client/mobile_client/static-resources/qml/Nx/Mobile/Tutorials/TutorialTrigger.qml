// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQml

import Nx.Core

import nx.vms.client.mobile

NxObject
{
    id: trigger

    enum Type { Available, Activated }

    property string name: ""
    property int when: TutorialTrigger.Available

    signal activated(Item item)

    InteractiveItemWatcher
    {
        name: trigger.name

        onAvailable: (item) =>
        {
            if (trigger.when === TutorialTrigger.Available)
                trigger.activated(item)
        }

        onInteracted: (item) =>
        {
            if (trigger.when === TutorialTrigger.Activated)
                trigger.activated(item)
        }
    }
}
