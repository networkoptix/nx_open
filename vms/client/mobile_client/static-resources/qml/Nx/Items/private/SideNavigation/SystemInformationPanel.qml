import QtQuick 2.6
import QtQuick.Layouts 1.1
import QtQuick.Controls 2.4
import Nx 1.0
import Nx.Items 1.0
import com.networkoptix.qml 1.0

Pane
{
    id: systemInformationPanel

    implicitWidth: parent ? parent.width : contentItem.implicitWidth
    implicitHeight: contentItem.implicitHeight + topPadding + bottomPadding
    padding: 0

    background: null

    QnCloudSystemInformationWatcher
    {
        id: cloudInformationWatcher
    }

    contentItem: SystemInformationBlock
    {
        topPadding: 16
        bottomPadding: 0
        systemName: connectionManager.systemName
        address: connectionManager.currentHost
        user: userWatcher.userName
        ownerDescription: cloudInformationWatcher.ownerDescription
        cloud: connectionManager.connectionType == QnConnectionManager.CloudConnection
    }
}
