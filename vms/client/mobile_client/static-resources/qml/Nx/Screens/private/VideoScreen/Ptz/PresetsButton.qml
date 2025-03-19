// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Controls
import Nx.Common
import Nx.Core
import Nx.Ui

Button
{
    id: control

    property Resource resource
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
                "resource": control.resource
            })

        dialog.onPresetChoosen.connect(control.presetChoosen)
    }
}
