// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.0
import Nx.Controls 1.0
import QtQuick.Layouts 1.3

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

    TextField
    {
        id: textField

        width: parent.width - backgroundRightOffset
        height: parent.height

        backgroundRightOffset: closeButton.visible ? closeButton.width : 0

        placeholderText: qsTr("Search")
        enterKeyType: TextInput.EnterKeySearch

        inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
        cursorColor: color

        onAccepted: Qt.inputMethod.hide()

        rightPadding: closeButton.width
    }

    IconButton
    {
        id: closeButton

        width: 48
        height: 48

        icon.source: lp("/images/clear.png")
        onClicked: textField.clear()

        opacity: textField.displayText && control.hasClearButton ? 1.0 : 0.0
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: 100 } }
        alwaysCompleteHighlightAnimation: false

        anchors.right: parent ? parent.right : undefined
        anchors.verticalCenter: parent ? parent.verticalCenter : undefined
    }
}
