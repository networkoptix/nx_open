// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Templates as T

import Nx.Core
import Nx.Core.Controls

import nx.vms.client.mobile

import "private"

// TODO #ynikitenkov Add "support text" functionality support.
T.TextField
{
    id: control

    property alias backgroundMode: fieldBackground.mode
    property alias labelText: fieldBackground.labelText
    property alias supportText: fieldBackground.supportText
    property alias errorText: fieldBackground.errorText

    property alias showActionButton: actionButton.visible
    property alias actionButtonImage: actionButton
    property var actionButtonAction:
        () =>
        {
            control.clear()
            control.errorText = ""
            control.forceActiveFocus()
        }

    implicitWidth: 268
    implicitHeight: 56 + fieldBackground.bottomTextHeight

    topPadding: 30
    leftPadding: 12
    rightPadding: actionButton.visible
        ? 8 + actionButton.width + actionButton.anchors.rightMargin
        : 12
    bottomInset: fieldBackground.bottomTextHeight

    font.pixelSize: 16
    font.weight: 500
    color: enabled
        ? ColorTheme.colors.light4
        : ColorTheme.transparent(ColorTheme.colors.light4, 0.3)
    selectionColor: ColorTheme.colors.brand_core

    background: FieldBackground
    {
        id: fieldBackground

        owner: control
    }

    onTextChanged: control.errorText = ""

    // TODO #ynikitenkov Add Nx.Mobile.IconButton
    ColoredImage
    {
        id: actionButton

        opacity: enabled ? 1 : 0.3
        visible: control.text && control.focus

        anchors.right: parent.right
        anchors.rightMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        sourcePath: "image://skin/24x24/Solid/cancel.svg?primary=light10"
        sourceSize: Qt.size(24, 24)

        MouseArea
        {
            anchors.fill: parent
            onClicked: control.actionButtonAction()
        }
    }
}
