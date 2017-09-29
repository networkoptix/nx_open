import QtQuick 2.6
import Nx 1.0
import Nx.Controls 1.0
import Nx.Items 1.0
import com.networkoptix.qml 1.0

import "private"

PageBase
{
    id: welcomeScreen

    padding: 16
    topPadding: 24

    header: Column
    {
        width: parent.width

        Rectangle
        {
            width: parent.width
            height: deviceStatusBarHeight
            color: ColorTheme.windowBackground
        }

        WarningPanel
        {
            id: warningPanel
        }
    }

    Flickable
    {
        id: flickable

        anchors.fill: parent

        contentWidth: content.width
        contentHeight: content.height

        Item
        {
            id: content

            width: flickable.width
            height: Math.max(flickable.height, column.height + skipButton.height)

            Column
            {
                id: column

                width: parent.width

                CloudBanner {}

                CredentialsEditor
                {
                    onLoggedIn: Workflow.openSessionsScreen()
                }
            }

            Button
            {
                id: skipButton
                text: qsTr("Skip")
                width: parent.width
                anchors.bottom: parent.bottom
                leftPadding: 0
                rightPadding: 0
                padding: 0

                onClicked: Workflow.openSessionsScreen()
            }
        }
    }
}
