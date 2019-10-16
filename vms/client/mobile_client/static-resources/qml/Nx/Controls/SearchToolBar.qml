import QtQuick 2.0
import QtQuick.Layouts 1.3
import Nx 1.0
import Nx.Controls 1.0

import "."

ToolBarBase
{
    id: toolBar

    property alias text: searchField.text

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
    }

    function open()
    {
        searchField.text = ""
        opacity = 1.0
        searchField.forceActiveFocus()
    }

    Keys.onPressed:
    {
        if (Utils.keyIsBack(event.key))
        {
            close()
            event.accepted = true
        }
    }
}
