// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Nx.Controls
import Nx.Dialogs

import nx.vms.client.core
import nx.vms.client.desktop

Dialog
{
    id: dialog

    property alias intensity: slider.value

    minimumWidth: implicitWidth
    maximumWidth: implicitWidth

    minimumHeight: implicitHeight
    maximumHeight: implicitHeight

    contentItem: Column
    {
        spacing: 8

        BlurMaskPreview
        {
            intensity: dialog.intensity
            imageSource: ":/skin/system_settings/pixelation_intensity_preview.png"
            blurRectangles: [
                Qt.rect(88, 229, 62, 32),
                Qt.rect(333, 17, 53, 72),
                Qt.rect(434, 9, 53, 72)
            ]
        }

        Row
        {
            Label
            {
                id: label

                text: qsTr("Intensity")
                leftPadding: 8
                rightPadding: 8
                topPadding: 6
                bottomPadding: 6
                font.pixelSize: FontConfig.normal.pixelSize
            }

            Slider
            {
                id: slider

                from: 0
                to: 3
                width: 208
                anchors.verticalCenter: label.verticalCenter
            }
        }

        Item { Layout.fillHeight: true }
    }

    buttonBox: DialogButtonBox
    {
        buttonLayout: DialogButtonBox.KdeLayout
        standardButtons: DialogButtonBox.Ok | DialogButtonBox.Cancel
    }

    WindowContextAware.onBeforeSystemChanged: reject()
}
