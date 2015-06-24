import QtQuick 2.4

import com.networkoptix.qml 1.0

Item {
    id: statusIndicator

    property int status

    width: dp(12)
    height: width

    property bool _offline: status == 0 || status == 4
    property bool _unauthorized: status == 1
    property bool _recording: status == 3

    Rectangle {
        anchors.fill: parent
        radius: width / 2

        color: !_offline && !_unauthorized && _recording ? QnTheme.cameraRecordingIndicator : "transparent"
        border.color: _offline || _unauthorized ? QnTheme.cameraOfflineText :
                                                  _recording ? QnTheme.cameraRecordingIndicator : QnTheme.cameraText
    }
}
