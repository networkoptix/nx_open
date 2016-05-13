import QtQuick 2.6
import Nx 1.0
import Nx.Controls 1.0

Item
{
    id: videoNavigation

    property var mediaPlayer

    implicitWidth: parent.width
    implicitHeight: content.height

    Image
    {
        width: parent.width
        height: sourceSize.height
        anchors.bottom: parent.bottom
        sourceSize.height: 56 * 2
        source: "qrc:///images/timeline_gradient.png"
    }

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

            running: mediaPlayer && mediaPlayer.loading
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
            font.pixelSize: 32
            font.weight: Font.Normal
            color: ColorTheme.windowText
            text: qsTr("LIVE")
            height: 64
            verticalAlignment: Text.AlignVCenter
            opacity: mediaPlayer && mediaPlayer.loading ? 0.2 : 1.0

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
}
