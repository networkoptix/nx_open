import QtQuick 2.6
import QtQuick.Layouts 1.1
import Qt.labs.controls 1.0
import Nx 1.0
import Nx.Items 1.0
import com.networkoptix.qml 1.0

Pane
{
    id: systemInformationPanel

    implicitWidth: parent ? parent.width : contentItem.implicitWidth
    implicitHeight: contentItem.implicitHeight + topPadding + bottomPadding
    padding: 0

    background: Item
    {
        Rectangle
        {
            width: parent.width - 32
            height: 1
            x: 16
            anchors.bottom: parent.bottom
            color: ColorTheme.base10
        }
    }

    QnCloudSystemInformationWatcher
    {
        id: cloudInformationWatcher
    }

    contentItem: SystemInformationBlock
    {
        topPadding: 16
        bottomPadding: 16
        systemName: connectionManager.systemName
        address: connectionManager.currentHost
        user: connectionManager.currentLogin
        ownerDescription: cloudInformationWatcher.ownerDescription
        cloud: connectionManager.cloudSystem
    }
}
