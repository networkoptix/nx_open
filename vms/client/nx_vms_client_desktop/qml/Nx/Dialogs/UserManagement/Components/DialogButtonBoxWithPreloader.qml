// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls

import Nx.Controls
import Nx.Core.Controls

DialogButtonBox
{
    id: buttonBox
    buttonLayout: DialogButtonBox.KdeLayout

    property bool modified: false
    standardButtons: DialogButtonBox.Ok | DialogButtonBox.Apply | DialogButtonBox.Cancel

    property var preloaderButton

    Component.onCompleted:
    {
        for (let buttonId of [DialogButtonBox.Ok, DialogButtonBox.Apply])
        {
            const button = buttonBox.standardButton(buttonId)

            if (!button)
                continue

            // Data that would be captured in binding closure.
            const data =
            {
                "id": buttonId,
                "text": button.text
            }

            button.text = Qt.binding(() => buttonBox.preloaderButton == data.id ? "" : data.text)
            button.enabled = Qt.binding(
                () => (data.id == DialogButtonBox.Apply
                        ? buttonBox.modified
                        : true)
                     && !buttonBox.preloaderButton)
        }
    }

    NxDotPreloader
    {
        anchors.centerIn: parent
        running: !!parent
        parent: buttonBox.preloaderButton
            ? buttonBox.standardButton(buttonBox.preloaderButton)
            : null
    }
}
