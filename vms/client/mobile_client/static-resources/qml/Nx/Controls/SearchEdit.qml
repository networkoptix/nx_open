// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Layouts

import Nx.Core
import Nx.Core.Controls
import Nx.Controls

Item
{
    id: control

    property alias placeholderText: textField.placeholderText

    readonly property alias displayText: textField.displayText
    property bool hasClearButton: true

    implicitWidth: textField.implicitWidth
    implicitHeight: textField.implicitHeight

    onActiveFocusChanged:
    {
        if (activeFocus)
            textField.forceActiveFocus()
    }

    function resetFocus()
    {
        textField.focus = false
    }

    function clear()
    {
        textField.clear()
    }

    signal accepted()

    ColoredImage
    {
        id: searchIcon

        sourceSize: Qt.size(20, 20)
        sourcePath: "image://skin/20x20/Outline/search.svg?primary=light16"

        anchors.verticalCenter: parent.verticalCenter
        x: 8
    }

    TextField
    {
        id: textField

        width: parent.width
        height: parent.height

        font.pixelSize: 14

        borderWidth: 1
        backgroundColor: "transparent"
        inactiveColor: ColorTheme.colors.dark6
        placeholderText: qsTr("Search")
        enterKeyType: TextInput.EnterKeySearch

        inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
        cursorColor: color

        onAccepted:
        {
            Qt.inputMethod.hide()
            control.accepted()
        }

        leftPadding: 32
        rightPadding: clearButton.visible ? 32 : 4
    }

    IconButton
    {
        id: clearButton

        width: 36
        height: 36
        padding: 0

        icon.width: 20
        icon.height: 20

        icon.source: "image://skin/20x20/Outline/close_small.svg?primary=light16"
        onClicked: textField.clear()

        opacity: textField.displayText && control.hasClearButton ? 1.0 : 0.0
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: 100 } }
        alwaysCompleteHighlightAnimation: false

        anchors.right: parent ? parent.right : undefined
        anchors.verticalCenter: parent ? parent.verticalCenter : undefined
    }
}
