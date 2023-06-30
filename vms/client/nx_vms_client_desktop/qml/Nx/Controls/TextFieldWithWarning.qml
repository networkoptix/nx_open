// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core 1.0

FocusScope
{
    id: root
    property alias warningText: warningMessage.text
    property alias readOnly: textField.readOnly
    property real warningSpacing: 8

    implicitWidth: textField.implicitWidth
    implicitHeight: textField.implicitHeight
        + (warningMessage.visible ? warningMessage.height + warningSpacing : 0)

    baselineOffset: textField.baselineOffset

    property alias textField: textField

    // Expose some frequently used properties to simplify the component usage. Other properties are
    // available via textField property.
    property alias text: textField.text
    property alias warningState: textField.warningState
    property alias cursorPosition: textField.cursorPosition

    signal textEdited()
    signal editingFinished()

    onFocusChanged: textField.focus = focus

    TextField
    {
        id: textField
        width: parent.width
        focus: true

        onTextEdited: parent.textEdited()
        onEditingFinished: root.editingFinished()
    }

    Text
    {
        id: warningMessage

        anchors.top: textField.bottom
        anchors.topMargin: warningSpacing
        width: parent.width

        color: ColorTheme.colors.red_l2
        font.pixelSize: 14
        wrapMode: Text.WordWrap

        visible: textField.warningState && text
    }
}
