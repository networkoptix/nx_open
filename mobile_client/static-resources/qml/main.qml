import QtQuick 2.2
import QtQuick.Window 2.0
import QtQuick.Controls 1.2

import com.networkoptix.qml 1.0

import "items"
import "controls"

import "main.js" as Main
import "items/QnLoginPage.js" as LoginFunctions

Window {
    id: mainWindow

    visible: true

    property string activeResourceId

    property string currentHost
    property int currentPort
    property string currentLogin
    property string currentPasswrod
    property string currentSessionId

    Rectangle {
        anchors.fill: parent
        color: QnTheme.windowBackground
    }

    QnToolBar {
        id: toolBar

        z: 5.0

        QnMenuBackButton {
            id: menuBackButton

            property bool transitionAnimated: false

            x: dp(10)
            anchors.verticalCenter: parent.verticalCenter

            Behavior on progress {
                enabled: menuBackButton.transitionAnimated
                NumberAnimation {
                    duration: 200
                    easing.type: Easing.OutCubic
                }
            }

            function animateToMenu() {
                transitionAnimated = true
                progress = 0.0
            }

            function animateToBack() {
                transitionAnimated = true
                progress = 1.0
            }

            onMenuOpenedChanged: transitionAnimated = false

            onClicked: {
                if (!menuOpened)
                    sideNavigation.show()
            }
        }
    }

    QnSideNavigation {
        id: sideNavigation
        activeSessionId: currentSessionId
    }

    StackView {
        id: stackView
        width: parent.width
        anchors.top: toolBar.bottom
        anchors.bottom: parent.bottom

        onCurrentItemChanged: {
            if (currentItem) {
                toolBar.title = Qt.binding(function() { return currentItem ? currentItem.title : "" })
            }
        }
    }

    Component {
        id: loginPageComponent

        QnLoginPage {
            objectName: "newConnectionPage"
        }
    }

    Component {
        id: resourcesPageComponent

        QnResourcesPage {
            id: resourcesPage
        }
    }

    Component {
        id: videoPlayerComponent

        QnVideoPage {
            id: videoPlayerContent
            resourceId: activeResourceId
        }
    }

    Component {
        id: settingsPageComponent

        QnSettingsPage {
        }
    }

    Connections {
        target: connectionManager

        onConnectedChanged: {
            if (connectionManager.connected) {
                LoginFunctions.saveCurrentSession()
                loginSessionManager.lastUsedSessionId = currentSessionId
                settings.sessionId = currentSessionId
                Main.gotoResources()
            } else {
                loginSessionManager.lastUsedSessionId = ""
                settings.sessionId = ""
                Main.gotoNewSession()
            }
        }

        onConnectionFailed: {
            if (stackView.depth > 1) /* we are in login page */
                return

            Main.openFailedSession(
                        currentSessionId,
                        currentHost, currentPort,
                        currentLogin, currentPasswrod,
                        loginSessionManager.lastUsedSessionSystemName(),
                        status, statusMessage)
        }
    }

    Connections {
        target: colorTheme
        onUpdated: QnTheme.loadColors()
    }

    Component.onCompleted: {
        currentSessionId = loginSessionManager.lastUsedSessionId
        if (currentSessionId) {
            stackView.push(resourcesPageComponent)
            LoginFunctions.connectToServer(
                        currentSessionId,
                        loginSessionManager.lastUsedSessionHost(), loginSessionManager.lastUsedSessionPort(),
                        loginSessionManager.lastUsedSessionLogin(), loginSessionManager.lastUsedSessionPassword())
        } else {
            stackView.push([resourcesPageComponent, loginPageComponent])
        }
    }
}
