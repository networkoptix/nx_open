import QtQuick 2.0
import Nx.Controls 1.0
import QtQuick.Layouts 1.3

TextField
{
    id: control

    placeholderText: qsTr("Search")
    enterKeyType: TextInput.EnterKeySearch

    inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
    cursorColor: color

    onAccepted: Qt.inputMethod.hide()

    rightPadding: closeButton.width

    IconButton
    {
        id: closeButton

        width: 48
        height: 48

        icon.source: lp("/images/clear.png")
        onClicked: parent.text= ""

        opacity: parent.text ? 1.0 : 0.0
        Behavior on opacity { NumberAnimation { duration: 100 } }
        alwaysCompleteHighlightAnimation: false

        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
    }
}
