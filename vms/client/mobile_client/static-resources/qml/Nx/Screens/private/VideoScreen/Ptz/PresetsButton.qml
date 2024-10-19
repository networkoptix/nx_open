// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6

import Nx.Core 1.0
import Nx.Controls 1.0
import Nx.Ui 1.0

Button
{
    id: control

    property var resourceId
    property Item popupParent

    signal presetChoosen(string id)
    height: 48
    flat: true
    labelPadding: 16

    text: qsTr("PRESETS")

    onClicked:
    {
        const dialog = Workflow.openDialog(Qt.resolvedUrl("PresetsDialog.qml"),
            {
                "resourceId": control.resourceId
            })

        dialog.onPresetChoosen.connect(control.presetChoosen)
    }
}
