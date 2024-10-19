// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6
import Nx.Dialogs 1.0

import nx.vms.client.core 1.0

DialogListItem
{
    id: control

    property int quality: 0

    enabled: qualityDialog.availableVideoQualities.indexOf(quality) !== -1
    active: qualityDialog.activeQuality == quality
    text: quality + 'p'

    onClicked:
    {
        qualityDialog.activeQuality = quality
        qualityDialog.close()
    }

    onActiveChanged:
    {
        if (active)
            qualityDialog.activeItem = this
    }
}
