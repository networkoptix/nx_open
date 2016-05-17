import QtQuick 2.6
import Qt.labs.controls 1.0
import QtQuick.Layouts 1.1
import Nx 1.0

Pane
{
    id: cloudPanel

    background: Rectangle
    {
        color: ColorTheme.base9

        Rectangle
        {
            width: parent.width
            height: 1
            anchors.bottom: parent.bottom
            color: ColorTheme.base7
        }
    }

    implicitWidth: parent ? parent.width : contentItem.implicitWidth
    implicitHeight: 48
    leftPadding: 12
    rightPadding: 12

    contentItem: RowLayout
    {
        spacing: 12

        Image
        {
            source: lp("/images/calendar.png") // TODO: #dklychkov Use proper icon
            anchors.verticalCenter: parent.verticalCenter
        }

        Text
        {
            Layout.fillWidth: true
            text: qsTr("Login to %1").arg(applicationInfo.cloudName())
            anchors.verticalCenter: parent.verticalCenter
            font.pixelSize: 14
            font.weight: Font.DemiBold
            elide: Text.ElideRight
            color: ColorTheme.contrast4
        }
    }
}
