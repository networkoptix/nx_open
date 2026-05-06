// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Nx.Core
import Nx.Core.Controls
import Nx.Controls
import Nx.Mobile.Controls
import Nx.Ui

AdaptiveSheet
{
    id: sheet

    property alias preloaders: panel.preloaders
    property alias controller: panel.controller
    property alias customRotation: panel.customRotation
    property alias moveOnTapMode: panel.moveOnTapMode
    property alias ptz: panel
    readonly property bool available: controller.available ?? false

    title: qsTr("PTZ")
    titleTextItem.font.pixelSize: 18

    alwaysShowCloseButton: true
    interactive: !panel.joystick.active
    spacing: 0
    bottomPadding: 0

    modal: false
    closePolicy: Popup.NoAutoClose

    PtzPanel
    {
        id: panel

        width: parent.width
        height: LayoutController.isPortrait ? implicitHeight : sheet.availableContentHeight
        visible: !sheet.moveOnTapMode
    }

    ColumnLayout
    {
        id: placeholder

        visible: sheet.moveOnTapMode
        width: parent.width
        height: panel.height

        spacing: 16

        Item { Layout.fillHeight: true }

        ColoredImage
        {
            sourcePath: "image://skin/48x48/Outline/re_centre.svg?primary=light10"
            sourceSize: Qt.size(48, 48)

            Layout.alignment: Qt.AlignCenter
        }

        Text
        {
            text: qsTr("Tap anywhere on video to center view there")
            color: ColorTheme.colors.light12
            font.pixelSize: 16
            wrapMode: Text.Wrap
            horizontalAlignment: Qt.AlignHCenter

            Layout.fillWidth: true
        }

        Item { Layout.fillHeight: true }

        Button
        {
            id: cancelButton

            text: qsTr("Cancel Re-Centering")
            type: Button.LightInterface
            onClicked: sheet.moveOnTapMode = false

            Layout.fillWidth: true
            Layout.bottomMargin: 24
        }
    }
}
