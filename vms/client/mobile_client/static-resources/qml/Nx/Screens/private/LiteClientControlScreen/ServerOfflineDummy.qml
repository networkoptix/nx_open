import QtQuick 2.6
import Nx 1.0
import Nx.Controls 1.0

Item
{
    Column
    {
        width: parent.width
        anchors.centerIn: parent

        Image
        {
            source: lp("/images/alert_nx1_offline.png")
            anchors.horizontalCenter: parent.horizontalCenter
        }

        Text
        {
            text: qsTr("%1 is offline").arg(applicationInfo.liteDeviceName())
            anchors.horizontalCenter: parent.horizontalCenter
            color: ColorTheme.base13
            font.pixelSize: 24
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
        }
    }
}

