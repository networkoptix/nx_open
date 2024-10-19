// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.0
import QtQuick.Layouts 1.3
import Nx.Core 1.0
import Nx.Controls 1.0

import "."

ToolBarBase
{
    id: toolBar

    readonly property alias text: searchField.displayText
    property alias cancelIcon: cancelButton.icon
    property alias hasClearButton: searchField.hasClearButton
    property bool closeOnEnter: false

    signal accepted()
    signal closed()

    opacity: 0.0
    Behavior on opacity { NumberAnimation { duration: 200 } }
    visible: opacity > 0
    enabled: visible

    RowLayout
    {
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        spacing: 8

        IconButton
        {
            id: cancelButton
            icon.source: lp("/images/arrow_back.png")
            onClicked: close()
            alwaysCompleteHighlightAnimation: false
        }

        SearchEdit
        {
            id: searchField

            height: 40
            Layout.fillWidth: true
            Layout.minimumHeight: height
        }
    }

    function close()
    {
        opacity = 0.0
        toolBar.closed()
    }

    function open()
    {
        searchField.clear()
        opacity = 1.0
        searchField.forceActiveFocus()
    }

    Keys.onPressed:
    {
        if (CoreUtils.keyIsBack(event.key))
        {
            close()
            event.accepted = true
        }
        else if (event.key == Qt.Key_Enter || event.key == Qt.Key_Return)
        {
            toolBar.accepted()
            event.accepted = true;
        }
    }
}
