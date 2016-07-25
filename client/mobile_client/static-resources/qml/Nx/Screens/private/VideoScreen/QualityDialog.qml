import QtQuick 2.6
import Qt.labs.controls 1.0
import Nx.Dialogs 1.0
import Nx 1.0
import com.networkoptix.qml 1.0

DialogBase
{
    id: qualityDialog

    property int activeQuality: QnPlayer.LowVideoQuality
    property size actualQuality

    implicitHeight: Math.min(parent.height, contentItem.implicitHeight)

    closePolicy: Popup.OnEscape | Popup.OnPressOutside | Popup.OnReleaseOutside
    deleteOnClose: true

    background: null

    contentItem: Flickable
    {
        id: flickable

        anchors.fill: parent

        implicitWidth: childrenRect.width
        implicitHeight: childrenRect.height

        contentWidth: width
        contentHeight: column.height

        topMargin: 16
        bottomMargin: 16
        contentY: 0

        Rectangle
        {
            anchors.fill: column
            color: ColorTheme.contrast3
        }

        Column
        {
            id: column

            width: flickable.width

            DialogTitle
            {
                text: qsTr("Video Quality")
                bottomPadding: 6
            }
            DialogMessage
            {
                text: {
                    if (actualQuality.width > 0 && actualQuality.height > 0)
                        return actualQuality.width + " x " + actualQuality.height
                    if (activeQuality != QnPlayer.LowVideoQuality && activeQuality != QnPlayer.HighVideoQuality)
                        return activeQuality + 'p'
                    return qsTr("Unknown")
                }
                font.pixelSize: 14
                bottomPadding: 12
            }

            DialogSeparator {}

            QualityItem
            {
                quality: QnPlayer.LowVideoQuality
                text: qsTr("Highest speed")
            }
            QualityItem
            {
                quality: QnPlayer.HighVideoQuality
                text: qsTr("Best quality")
            }

            DialogSeparator {}

            Repeater
            {
                model: [ 1080, 720, 480, 360 ]
                QualityItem { quality: modelData }
            }
        }
    }
}
