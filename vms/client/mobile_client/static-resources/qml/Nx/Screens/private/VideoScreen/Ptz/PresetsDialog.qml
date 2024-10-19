// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6

import Nx.Controls 1.0
import Nx.Core 1.0
import Nx.Mobile 1.0
import Nx.Dialogs 1.0

DialogBase
{
    id: control

    property var resourceId

    signal presetChoosen(string id)

    Column
    {
        width: parent.width

        DialogTitle
        {
            text: qsTr("PTZ Presets")
        }

        DialogSeparator {}

        Repeater
        {
            model: PtzPresetModel
            {
                resourceId: control.resourceId
            }

            delegate: DialogListItem
            {
                text: (index + 1) + ". " + display
                onClicked:
                {
                    control.presetChoosen(id)
                    control.close()
                }
            }
        }
    }
}
