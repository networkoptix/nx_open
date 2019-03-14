import QtQuick 2.6
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.1
import Nx 1.0
import Nx.Controls 1.0

Pane
{
    id: cloudPanel

    clip: true

    readonly property string login: cloudStatusWatcher.effectiveUserName

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

        MaterialEffect
        {
            anchors.fill: parent
            mouseArea: mouseArea
            rippleSize: 160
        }
    }

    implicitWidth: parent ? parent.width : contentItem.implicitWidth
    implicitHeight: 48
    leftPadding: 12
    rightPadding: 12

    MouseArea
    {
        id: mouseArea
        anchors.fill: parent
        onClicked: Workflow.openCloudScreen()
    }

    contentItem: Item
    {
        Image
        {
            id: cloudStatusImage

            source: login ? lp("/images/cloud_logged_in.png")
                          : lp("/images/cloud_not_logged_in.png")
            anchors.verticalCenter: parent.verticalCenter
        }

        Text
        {
            x: cloudStatusImage.x + cloudStatusImage.width + 12
            width: parent.width - x
            text: login
                ? login
                : qsTr("Log in to %1").arg(applicationInfo.cloudName())
            anchors.verticalCenter: parent.verticalCenter
            font.pixelSize: 14
            font.weight: Font.DemiBold
            elide: Text.ElideRight
            color: ColorTheme.contrast10
        }
    }
}
