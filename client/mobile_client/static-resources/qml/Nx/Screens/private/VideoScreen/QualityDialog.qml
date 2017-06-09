import QtQuick 2.6
import Qt.labs.controls 1.0
import Nx.Dialogs 1.0
import Nx 1.0
import Nx.Media 1.0
import com.networkoptix.qml 1.0

DialogBase
{
    id: qualityDialog

    property int activeQuality: MediaPlayer.LowVideoQuality
    property size actualQuality
    property var customQualities: []
    property var availableVideoQualities: []

    deleteOnClose: true

    Column
    {
        width: parent.width

        DialogTitle
        {
            text: qsTr("Video Quality")
            bottomPadding: 6
        }
        DialogMessage
        {
            text:
            {
                if (actualQuality.width > 0 && actualQuality.height > 0)
                    return actualQuality.width + " x " + actualQuality.height

                if (activeQuality !== MediaPlayer.LowVideoQuality
                    && activeQuality !== MediaPlayer.HighVideoQuality)
                {
                    return activeQuality + 'p'
                }

                return qsTr("Unknown")
            }
            font.pixelSize: 14
            bottomPadding: 12
        }

        DialogSeparator {}

        QualityItem
        {
            quality: MediaPlayer.LowVideoQuality
            text: qsTr("Highest speed")
        }
        QualityItem
        {
            quality: MediaPlayer.HighVideoQuality
            text: qsTr("Best quality")
        }

        DialogSeparator {}

        Repeater
        {
            model: customQualities
            QualityItem { quality: modelData }
        }
    }
}
