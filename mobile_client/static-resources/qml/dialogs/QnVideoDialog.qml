import QtQuick 2.2
import QtMultimedia 5.0

import com.networkoptix.qml 1.0

Item {

    property alias resourceId: resourceHelper.resourceId

    property var __syspal: SystemPalette {
        colorGroup: SystemPalette.Active
    }

    QnMediaResourceHelper {
        id: resourceHelper
    }

    /* backgorund */
    Rectangle {
        anchors.fill: parent
        color: __syspal.window
    }

    Video {
        id: video

        anchors.fill: parent
        source: resourceHelper.mediaUrl
        autoPlay: true
    }
}
