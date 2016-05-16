import QtQuick 2.0
import Nx 1.0
import Nx.Controls 1.0
import com.networkoptix.qml 1.0

SideNavigationItem
{
    id: sessionItem

    property string sessionId
    property string systemName
    property string address
    property int port
    property string user
    property string password

    implicitWidth: 200
    implicitHeight: contentItem.implicitHeight + topPadding + bottomPadding
    topPadding: 12
    bottomPadding: 12
    leftPadding: 16
    rightPadding: 16

    contentItem: Column
    {
        width: parent.width

        Text
        {
            text: systemName
            font.pixelSize: 18
            font.weight: Font.DemiBold
            color: ColorTheme.windowText
            elide: Text.ElideRight
            width: parent.width - editButton.width
            height: 32
            verticalAlignment: Text.AlignVCenter

            IconButton
            {
                id: editButton

                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.right
                anchors.leftMargin: 8
                icon: lp("/images/edit.png")

                onClicked:
                {
                    sideNavigation.close()
                    Workflow.openSavedSession(sessionId, systemName, address, port, user, password)
                }
            }
        }

        Text
        {
            width: parent.width
            height: 24

            text: address + ":" + port
            font.pixelSize: 15
            font.weight: Font.DemiBold
            color: ColorTheme.contrast2
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
        }

        Text
        {
            width: parent.width
            height: 24

            text: user
            font.pixelSize: 15
            font.weight: Font.DemiBold
            color: ColorTheme.contrast2
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
        }
    }

    onClicked:
    {
        sideNavigation.close()

        if (active)
            return

        currentSessionId = sessionId
        connectionManager.connectToServer(LoginUtils.makeUrl(address, port, user, password))
        Workflow.openResourcesScreen(systemName)
    }
}
