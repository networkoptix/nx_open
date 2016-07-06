import QtQuick 2.0
import Qt.labs.controls 1.0
import Nx 1.0
import com.networkoptix.qml 1.0

Button
{
    id: control

    implicitWidth: parent ? parent.width : 200
    implicitHeight: 48

    text: applicationInfo.version()
    background: null
    leftPadding: 16
    rightPadding: 16
    font.pixelSize: 13

    onDoubleClicked: copyToClipboard(versionText.text)

    label: Text
    {
        id: versionText

        anchors.fill: control
        anchors.leftMargin: control.leftPadding
        anchors.rightMargin: control.rightPadding

        verticalAlignment: Text.AlignVCenter
        font.pixelSize: 13
        color: ColorTheme.base15
        text: control.text
    }
}

