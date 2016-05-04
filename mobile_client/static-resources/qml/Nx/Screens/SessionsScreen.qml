import QtQuick 2.6
import Nx 1.0
import Nx.Controls 1.0
import Nx.Items 1.0
import com.networkoptix.qml 1.0

Page
{
    id: sessionsScreen

    leftButtonIcon: "/images/menu.png"
    padding: 16

    SideNavigation { id: sideNavigation }
    onLeftButtonClicked: sideNavigation.open()

    ListView
    {
        id: sessionsList

        anchors.fill: parent
        spacing: 1

        model: QnDiscoveredSessionsModel { id: discoveredSessionsModel }
        delegate: SessionItem
        {
            width: sessionsList.width
            systemName: model.systemName
            host: model.address
            port: model.port
            version: model.serverVersion
            compatible: model.isCompatible
        }
        displayMarginBeginning: 16
        displayMarginEnd: 16 + mainWindow.bottomPadding

        footer: Item
        {
            width: parent.width
            height: customConnectionButton.y + customConnectionButton.height

            Button
            {
                id: customConnectionButton

                text: qsTr("Connect to Another Server")

                anchors.horizontalCenter: parent.horizontalCenter
                width: getDeviceIsPhone() ? parent.availableWidth : implicitWidth
                y: 10

                onClicked: Workflow.openNewSessionScreen()
            }
        }
    }
}
