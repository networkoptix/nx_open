import QtQuick 2.0
import Nx 1.0
import com.networkoptix.qml 1.0

Rectangle
{
    id: statusIndicator

    property int status

    property bool _offline: status == QnCameraListModel.Offline ||
                            status == QnCameraListModel.NotDefined
    property bool _unauthorized: status == QnCameraListModel.Unauthorized
    property bool _recording: status == QnCameraListModel.Recording

    implicitWidth: 10
    implicitHeight: implicitWidth
    radius: height / 2

    color: !_offline && !_unauthorized && _recording ? ColorTheme.red_main : "transparent"
    border.color: (_offline || _unauthorized) ? ColorTheme.base11
                                              : _recording ? ColorTheme.red_main
                                                           : ColorTheme.windowText
}
