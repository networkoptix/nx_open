import QtQuick 2.0
import QtQuick.Layouts 1.3
import Nx.Controls 1.0

// TODO: #dklychkov remove
import "../../controls"

ToolBarBase
{
    id: toolBar

    property alias text: searchField.text

    statusBarHeight: 0

    opacity: 0.0
    Behavior on opacity { NumberAnimation { duration: 200 } }
    visible: opacity > 0

    RowLayout
    {
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        spacing: 8

        IconButton
        {
            icon: "/images/arrow_back.png"
            onClicked: close()
            alwaysCompleteHighlightAnimation: false
        }

        QnTextField
        {
            id: searchField

            Layout.fillWidth: true

            placeholderText: qsTr("Search")

            showDecoration: false
            inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
            cursorColor: textColor
        }

        IconButton
        {
            icon: "/images/clear.png"
            onClicked: clear()
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
}
