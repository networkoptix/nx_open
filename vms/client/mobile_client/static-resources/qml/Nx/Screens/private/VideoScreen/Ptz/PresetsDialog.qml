// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Controls
import Nx.Common
import Nx.Core
import Nx.Mobile
import Nx.Dialogs

DialogBase
{
    id: control

    property Resource resource

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
                resource: control.resource
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
