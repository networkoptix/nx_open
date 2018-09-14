import QtQuick 2.6
import QtQuick.Controls 2.4
import Nx 1.0
import com.networkoptix.qml 1.0
import QtQuick.Window 2.2

import "private/SideNavigation"

Drawer
{
    id: sideNavigation

    readonly property real offset:
        Screen.orientation === Qt.LandscapeOrientation ? leftCustomMargin : 0

    position: 0
    enabled: stackView.depth === 1
    width: offset + Math.min(
        ApplicationWindow.window.width - 56,
        ApplicationWindow.window.height - 56,
        56 * 6)
    height: ApplicationWindow.window.height - y

    Overlay.modal: Rectangle
    {
        color: ColorTheme.backgroundDimColor
        Behavior on opacity { NumberAnimation { duration: 200 } }
    }

    Rectangle
    {
        anchors.fill: parent
        color: ColorTheme.base8
        clip: true

        Item
        {
            x: sideNavigation.offset
            width: parent.width - x
            height: parent.height

            ListView
            {
                id: layoutsList

                anchors.fill: parent
                anchors.bottomMargin: bottomContent.height
                bottomMargin: 8
                flickableDirection: Flickable.VerticalFlick
                clip: true

                header: Column
                {
                    width: layoutsList.width

                    CloudPanel { }

                    SystemInformationPanel
                    {
                        visible: connectionManager.online
                    }
                }

                model: connectionManager.online ? layoutsModel : undefined

                delegate: LayoutItem
                {
                    text: resourceName
                    resourceId: uuid
                    shared: shared
                    active: uiController.layoutId === resourceId
                    type: itemType
                    count: itemsCount
                    onClicked:
                    {
                        if (itemType == QnLayoutsModel.LiteClient)
                            Workflow.openLiteClientControlScreen(resourceId)
                        else
                            Workflow.openResourcesScreen()

                        uiController.layoutId = resourceId
                    }
                }

                section.property: "section"
                section.labelPositioning: ViewSection.InlineLabels
                section.delegate: Item
                {
                    width: layoutsList.width
                    height: 32

                    Rectangle
                    {
                        anchors.centerIn: parent
                        width: parent.width - 32
                        height: 1
                        color: ColorTheme.base10
                    }
                }
            }

            QnLayoutsModel { id: layoutsModel }

            OfflineDummy
            {
                anchors.fill: layoutsList
                anchors.margins: 16
                visible: connectionManager.connectionState === QnConnectionManager.Disconnected
            }

            Column
            {
                id: bottomContent

                width: parent.width
                bottomPadding: mainWindow.bottomPadding + sideNavigation.y
                anchors.bottom: parent.bottom

                BottomSeparator
                {
                    visible: connectionManager.online
                }

                SideNavigationButton
                {
                    icon: lp("/images/plus.png")
                    text: qsTr("New connection")
                    visible: connectionManager.connectionState === QnConnectionManager.Disconnected
                    onClicked:
                    {
                        bottomContent.enabled = false
                        sideNavigation.close()
                        Workflow.openNewSessionScreen()
                    }
                }

                SideNavigationButton
                {
                    id: disconnectButton

                    icon: lp("/images/disconnect.png")
                    text: qsTr("Disconnect from Server")
                    visible: connectionManager.connectionState !== QnConnectionManager.Disconnected
                    onClicked:
                    {
                        bottomContent.enabled = false
                        sideNavigation.close()
                        uiController.disconnectFromSystem()
                    }
                }

                SideNavigationButton
                {
                    icon: lp("/images/settings.png")
                    text: qsTr("Settings")
                    onClicked:
                    {
                        bottomContent.enabled = false
                        sideNavigation.close()
                        Workflow.openSettingsScreen()
                    }
                }

                SideNavigationButton
                {
                    text: qsTr("Start test")
                    visible: testMode && !testLoader.item
                    onClicked: Workflow.openDialog("Test/TestSelectionDialog.qml")
                }

                SideNavigationButton
                {
                    text: qsTr("Stop test")
                    visible: testMode && testLoader.item
                    onClicked: Workflow.stopTest()
                }

                VersionLabel {}
            }
        }
    }

    onOpenedChanged:
    {
        if (opened)
        {
            bottomContent.enabled = true

            if (liteMode)
            {
                // TODO: #dklychkov Implement proper focus control
                disconnectButton.forceActiveFocus()
            }
            else
            {
                forceActiveFocus()
            }
        }
        else
        {
            Workflow.focusCurrentScreen()
        }
    }

    Keys.onPressed:
    {
        if (Utils.keyIsBack(event.key))
        {
            close()
            event.accepted = true
        }
    }
}
