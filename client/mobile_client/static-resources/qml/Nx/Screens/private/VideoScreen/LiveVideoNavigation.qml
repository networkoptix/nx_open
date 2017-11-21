import QtQuick 2.6
import Nx 1.0
import Nx.Media 1.0
import Nx.Controls 1.0

Item
{
    id: videoNavigation

    property var videoScreenController
    property bool ptzAvailable: false

    signal ptzButtonClicked()

    implicitWidth: parent.width
    implicitHeight: content.height

    Column
    {
        id: content

        width: parent.width

        CircleBusyIndicator
        {
            width: 32
            height: 32
            anchors.horizontalCenter: parent.horizontalCenter
            lineWidth: 2
            color: ColorTheme.windowText

            running: videoScreenController && videoScreenController.mediaPlayer.loading
            opacity: running ? 1.0 : 0.0

            Behavior on opacity
            {
                SequentialAnimation
                {
                    PauseAnimation { duration: 250 }
                    NumberAnimation { duration: 250 }
                }
            }
        }

        Text
        {
            id: liveLabel
            anchors.horizontalCenter: parent.horizontalCenter
            font.pixelSize: 28
            font.weight: Font.Normal
            color: ColorTheme.windowText
            text: qsTr("LIVE")
            height: 56
            verticalAlignment: Text.AlignVCenter
            opacity: videoScreenController
                && videoScreenController.mediaPlayer.loading ? 0.2 : 1.0

            Behavior on opacity
            {
                SequentialAnimation
                {
                    PauseAnimation { duration: 250 }
                    NumberAnimation { duration: 250 }
                }
            }
        }
    }

    IconButton
    {
        icon: lp("images/ptz/ptz.png")
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.rightMargin: 4
        anchors.bottomMargin: 4
        visible: videoNavigation.ptzAvailable

        onClicked: videoNavigation.ptzButtonClicked()
    }
}
