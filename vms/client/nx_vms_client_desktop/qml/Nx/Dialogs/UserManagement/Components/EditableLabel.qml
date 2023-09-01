// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Window

import Nx
import Nx.Core
import Nx.Controls

import nx.vms.client.desktop

Item
{
    id: control

    property var font: Qt.font({pixelSize: 24, weight: Font.Medium})

    property bool enabled: true
    onEnabledChanged: control.forceFinishEdit()

    property alias text: labelTextField.text
    property alias validateFunc: labelTextField.validateFunc

    readonly property alias editing: labelTextField.visible

    property string originalText: text

    height: editing && labelTextField.warningState
        ? labelTextField.y + labelTextField.height
        : label.height

    readonly property bool hovered: labelMouseArea.containsMouse || editButton.hovered

    Text
    {
        id: label

        textFormat: Text.PlainText
        font: control.font
        color: control.hovered ? ColorTheme.colors.light1 : ColorTheme.colors.light4
        text: labelTextField.text
        elide: Text.ElideRight

        width: Math.min(label.implicitWidth, parent.width - editButton.horizontalSpace)

        MouseArea
        {
            id: labelMouseArea
            anchors.fill: parent
            anchors.rightMargin: - (editButton.anchors.leftMargin + editButton.width)
            enabled: control.enabled
            hoverEnabled: true
            onClicked: control.startEdit()
        }
    }

    ImageButton
    {
        id: editButton

        readonly property real horizontalSpace: visible ? width + anchors.leftMargin : 0

        anchors.left: label.right
        anchors.leftMargin: 4
        anchors.verticalCenter: label.verticalCenter

        width: 20
        height: 20

        visible: label.visible && control.enabled && control.hovered
        hoverEnabled: true
        enabled: control.enabled

        icon.source: "image://svg/skin/user_settings/edit_filled.svg"
        icon.width: width
        icon.height: height
        icon.color: hovered ? ColorTheme.colors.light13 : ColorTheme.colors.light16

        background: null

        onClicked: control.startEdit()
    }

    TextFieldWithValidator
    {
        id: labelTextField

        textField.height: 32
        visible: false
        textField.font: control.font
        textField.padding: 2

        anchors.left: parent.left
        anchors.leftMargin: - textField.leftPadding
        anchors.top: parent.top
        anchors.topMargin: - textField.topPadding

        anchors.right: parent.right

        onFocusChanged:
        {
            if (focus)
                return

            finishEdit()
        }
    }

    Instrument
    {
        item: control.Window.window
        onMousePress: mouse =>
        {
            const pos = labelTextField.mapFromGlobal(mouse.globalPosition)

            if (pos.x < 0 || pos.y < 0 || pos.x > labelTextField.width
                || pos.y > labelTextField.height)
            {
                control.finishEdit()
            }
        }
    }

    function validate()
    {
        if (!enabled)
            return true

        // When edited text is empty just revert it to original value.
        if (!labelTextField.text)
            labelTextField.text = control.originalText

        const success = labelTextField.validate()

        // Switch to editing to show validation error.
        if (!success && !control.editing)
            startEdit()

        return success
    }

    function startEdit()
    {
        label.visible = false
        control.originalText = labelTextField.text
        labelTextField.visible = true
        labelTextField.focus = true
    }

    function forceFinishEdit()
    {
        if (!control.editing)
            return

        // Switch state.
        labelTextField.focus = false
        labelTextField.visible = false
        if (!labelTextField.text)
            labelTextField.text = control.originalText
        label.visible = true
    }

    function finishEdit()
    {
        if (!control.editing)
            return

        if (labelTextField.text && !validate())
            return

        forceFinishEdit()
    }

    Connections
    {
        target: control.Window.window
        function onVisibilityChanged() { control.finishEdit() }
    }
}
