import QtQuick 2.6
import Nx 1.0
import com.networkoptix.qml 1.0

Rectangle
{
    color: ColorTheme.base3

    Image
    {
        anchors.centerIn: parent
        // TODO: #dklychkov use a proper icon
        source: lp("images/camera_offline_1.png")
    }
}
