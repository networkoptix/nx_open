import QtQuick 2.6
import QtQuick.Layouts 1.1
import Qt.labs.controls 1.0
import Nx 1.0
import Nx.Controls 1.0
import com.networkoptix.qml 1.0

Pane
{
    id: control

    property alias systemId: informationBlock.systemId
    property alias systemName: informationBlock.systemName
    property alias cloudSystem: informationBlock.cloud
    property alias online: informationBlock.online
    property alias compatible: informationBlock.compatible
    property alias ownerDescription: informationBlock.ownerDescription
    property alias invalidVersion: informationBlock.invalidVersion

    padding: 0

    implicitHeight: contentItem.implicitHeight + topPadding + bottomPadding
    implicitWidth: 200

    QnSystemHostsModel
    {
        id: hostsModel
        systemId: control.systemId
    }
    QnRecentUserConnectionsModel
    {
        id: connectionsModel
        systemName: control.systemName
    }

    background: Rectangle
    {
        color: control.enabled ? ColorTheme.base7 : ColorTheme.base6
        radius: 2

        MaterialEffect
        {
            anchors.fill: parent
            mouseArea: mouseArea
            clip: true
            rippleSize: width * 0.8
        }
    }

    MouseArea
    {
        id: mouseArea

        anchors.fill: parent
        onClicked: open()
    }

    contentItem: SystemInformationBlock
    {
        id: informationBlock
        enabled: compatible && online
        address: hostsModel.firstHost
        user: connectionsModel.firstUser
    }

    IconButton
    {
        /* It should not be parented by contentItem */
        parent: control

        y: 2
        anchors.right: parent.right
        icon: lp("/images/edit.png")
        visible: connectionsModel.hasConnections
        onClicked:
        {
            Workflow.openSavedSession(systemName,
                                      informationBlock.address,
                                      informationBlock.user,
                                      connectionsModel.getData("password", 0))
        }
    }

    function open()
    {
        if (!contentItem.enabled)
            return

        if (cloudSystem)
        {
            if (!hostsModel.isEmpty)
            {
                connectionManager.connectToServer(
                            LoginUtils.makeUrl(hostsModel.firstHost,
                                               cloudStatusWatcher.cloudLogin,
                                               cloudStatusWatcher.cloudPassword,
                                               true))
                Workflow.openResourcesScreen(systemName)
            }
        }
        else
        {
            if (connectionsModel.hasConnections)
            {
                connectionManager.connectToServer(
                            LoginUtils.makeUrl(informationBlock.address,
                                               informationBlock.user,
                                               connectionsModel.getData("password", 0)))
                Workflow.openResourcesScreen(systemName)
            }
            else
            {
                Workflow.openDiscoveredSession(systemName, informationBlock.address)
            }
        }
    }

    Keys.onEnterPressed: open()
    Keys.onReturnPressed: open()
}
