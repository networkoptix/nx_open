import QtQuick 2.6
import Nx 1.0
import Nx.Controls 1.0

FocusScope
{
    id: loginButton

    property alias text: connectButton.text
    property bool showProgress: false

    signal clicked()

    implicitWidth: connectButton.implicitWidth
    implicitHeight: connectButton.implicitHeight

    Button
    {
        id: connectButton

        anchors.fill: parent
        text: qsTr("Connect")
        color: ColorTheme.brand_main
        textColor: ColorTheme.brand_contrast
        focus: true
        onClicked: loginButton.clicked()
        opacity: showProgress ? 0.0 : 1.0
        Behavior on opacity { NumberAnimation { duration: 200 } }
        padding: 0
    }

    ThreeDotBusyIndicator
    {
        anchors.centerIn: parent
        opacity: 1.0 - connectButton.opacity
        running: opacity > 0
    }
}
