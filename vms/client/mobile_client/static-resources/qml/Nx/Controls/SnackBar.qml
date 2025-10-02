// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.0
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.15

import Nx.Controls 1.0
import Nx.Mobile.Controls 1.0
import Nx.Core 1.0

Popup
{
    id: control

    readonly property int gap: 20

    property alias text: message.text
    property alias buttonText: actionButton.text
    property alias withCloseButton: closeButton.visible

    signal clicked

    x: gap
    height: grid.implicitHeight + topPadding + bottomPadding
    width: parent ? parent.width - (gap * 2) : 0
    padding: 16
    closePolicy: Popup.NoAutoClose

    onVisibleChanged:
    {
        if (visible)
            closeTimer.start()
        else
            closeTimer.stop()
    }

    Timer
    {
        id: closeTimer
        interval: 5000
        onTriggered: control.close()
    }

    background: Rectangle
    {
        color: ColorTheme.colors.dark10
        radius: 4
    }

    contentItem: Item
    {
        GridLayout
        {
            id: grid
            width: parent.width
            columns: message.isLong && buttonControlsLayout.isLong ? 1 : 2

            Text
            {
                id: message
                property bool isLong: message.implicitWidth > grid.width - buttonControlsLayout.width - grid.columnSpacing
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                font.pixelSize: 16
                font.weight: Font.Medium
                color: ColorTheme.colors.light10
            }

            RowLayout
            {
                id: buttonControlsLayout
                property bool isLong: width > 100
                Layout.alignment: Qt.AlignRight

                TextButton
                {
                    id: actionButton
                    Layout.preferredHeight: 24
                    onClicked: control.clicked()
                    visible: text
                }

                IconButton
                {
                    id: closeButton
                    Layout.preferredWidth: 24
                    Layout.preferredHeight: 24
                    icon.source: "image://skin/24x24/Outline/close.svg?primary=light16"
                    icon.width: 24
                    icon.height: 24

                    onClicked: control.close()
                }
            }
        }
    }
}
