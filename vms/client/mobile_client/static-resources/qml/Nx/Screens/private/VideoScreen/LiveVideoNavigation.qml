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

        Item
        {
            id: holder

            width: parent.width
            height: 56

            readonly property real minimalWidth: holder.width - (liveLabel.x + liveLabel.width)
            readonly property bool showLiveLabel: actionButtonsPanel.contentWidth < minimalWidth

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
                opacity:
                {
                    if (!holder.showLiveLabel)
                        return 0.0;

                    return videoScreenController && videoScreenController.mediaPlayer.loading
                        ? 0.2
                        : 1.0
                }

                Behavior on opacity
                {
                    SequentialAnimation
                    {
                        PauseAnimation { duration: 250 }
                        NumberAnimation { duration: 250 }
                    }
                }
            }

            ActionButtonsPanel
            {
                id: actionButtonsPanel

                resourceId: videoScreenController.resourceId

                anchors.verticalCenter: parent.verticalCenter
                anchors.left: holder.showLiveLabel ? liveLabel.right : holder.left
                anchors.right: parent.right
                anchors.leftMargin: 4
                anchors.rightMargin: 4

                onPtzButtonClicked: videoNavigation.ptzButtonClicked()
                onTwoWayAudioButtonPressed: twoWayAudioController.start()
                onTwoWayAudioButtonReleased: twoWayAudioController.stop()

                Behavior on opacity { NumberAnimation { duration: 200 } }

                Binding
                {
                    target: twoWayAudioController
                    property: "resourceId"
                    value: videoScreenController.resourceId
                }
            }
        }
    }
}
