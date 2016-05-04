import QtQuick 2.0
import Qt.labs.controls 1.0
import Nx 1.0
import com.networkoptix.qml 1.0

import "private/SideNavigation"

Drawer
{
    id: sideNavigation

    position: 0

    Rectangle
    {
        width: Math.min(mainWindow.width - 56, mainWindow.height - 56, 56 * 6)
        height: mainWindow.height
        color: ColorTheme.base8

        ListView
        {
            id: savedSessionsList

            anchors.fill: parent
            anchors.topMargin: getStatusBarHeight()
            anchors.bottomMargin: bottomContent.height - 16
            flickableDirection: Flickable.VerticalFlick
            clip: true

            model: savedSessionsModel
            delegate: SessionItem
            {
                width: savedSessionsList.width
                active: currentSessionId == sessionId

                sessionId: model.sessionId
                systemName: model.systemName
                address: model.address
                port: model.port
                user: model.user
                password: model.password
            }

            header: SavedSessionsHeader {}
        }

        Column
        {
            id: bottomContent

            width: parent.width
            anchors.bottom: parent.bottom

            SideNavigationSeparator {}

            SideNavigationButton
            {
                icon: "/images/plus.png"
                text: qsTr("New connection")
                active: !currentSessionId
                onClicked:
                {
                    sideNavigation.close()
//                    Main.gotoNewSession()
//                    stackView.currentItem.scrollTop()
                }
            }

            SideNavigationButton
            {
                icon: "/images/settings.png"
                text: qsTr("Settings")
                visible: !liteMode
                onClicked: Workflow.openSettingsScreen()
            }

            VersionLabel {}
        }
    }
}
