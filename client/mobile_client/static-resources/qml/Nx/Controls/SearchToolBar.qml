import QtQuick 2.0
import QtQuick.Layouts 1.3
import Nx 1.0
import Nx.Controls 1.0

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
            icon: lp("/images/arrow_back.png")
            onClicked: close()
            alwaysCompleteHighlightAnimation: false
        }

        TextField
        {
            id: searchField

            Layout.fillWidth: true

            placeholderText: qsTr("Search")
            enterKeyType: TextInput.EnterKeySearch

            background: null
            inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
            cursorColor: color

            onAccepted: Qt.inputMethod.hide()
        }

        IconButton
        {
            icon: lp("/images/clear.png")
            onClicked: clear()
            opacity: searchField.text ? 1.0 : 0.0
            Behavior on opacity { NumberAnimation { duration: 100 } }
            alwaysCompleteHighlightAnimation: false
        }
    }

    function clear()
    {
        searchField.text = ""
    }

    function close()
    {
        opacity = 0.0
    }

    function open()
    {
        clear()
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
