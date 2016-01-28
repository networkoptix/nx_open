import QtQuick 2.4
import QtQuick.Window 2.2
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
    property string initialResourceScreenshot

    property string currentSystemName
    property string currentHost
    property int currentPort
    property string currentLogin
    property string currentPasswrod
    property string currentSessionId
    property bool customConnection: false

    readonly property bool isPhone: getDeviceIsPhone()

    color: QnTheme.windowBackground

    QnToolBar {
        id: toolBar

        width: mainWindow.width - navigationBarPlaceholder.width

        QnMenuBackButton {
            id: menuBackButton

            parent: toolBar.contentItem

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

    Item {
        id: navigationBarPlaceholder

        property real realWidth: 0
        property real realHeight: 0

        width: realWidth == 0 ? 1 : realWidth
        height: realHeight == 0 ? 1 : realHeight

        anchors.bottom: parent.bottom
        anchors.right: parent.right
    }

    QnSideNavigation {
        id: sideNavigation
        activeSessionId: currentSessionId
    }

    QnMainStackView {
        id: stackView

        anchors.top: toolBar.bottom
        anchors.bottom: parent.bottom
        anchors.bottomMargin: navigationBarPlaceholder.realHeight
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.rightMargin: navigationBarPlaceholder.realWidth

        onCurrentItemChanged: {
            if (currentItem) {
                toolBar.title = Qt.binding(function() { return currentItem ? currentItem.title : "" })
                currentItem.forceActiveFocus()
            }
        }
    }

    Item {
        id: overlayBound

        anchors.fill: parent
        anchors.topMargin: toolBar.statusBarHeight
        anchors.bottomMargin: navigationBarPlaceholder.realHeight
        anchors.rightMargin: navigationBarPlaceholder.realWidth

        z: 10.0

        QnOverlayLayer {
            id: dialogLayer
            z: 10.0
            color: "#30000000"
        }

        QnOverlayLayer {
            id: toastLayer
            z: 15.0
            blocking: false
        }

        QnOverlayLayer {
            id: popupLayer
            z: 20.0
        }

        QnOverlayLayer {
            id: widgetLayer
            z: 25.0
            color: "#66171c1f"
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
            initialScreenshot: initialResourceScreenshot
        }
    }

    Component {
        id: settingsPageComponent

        QnSettingsPage {
        }
    }

    Connections {
        target: connectionManager

        onConnectionStateChanged: {
            var connectionState = connectionManager.connectionState
            if (connectionState == QnConnectionManager.Connected) {
                LoginFunctions.saveCurrentSession()
                loginSessionManager.lastUsedSessionId = currentSessionId
                settings.sessionId = currentSessionId
                currentSystemName = connectionManager.systemName
                if (stackView.currentItem.objectName == "newConnectionPage" || stackView.currentItem.objectName == "loginPage") {
                    stackView.setFadeTransition()
                    Main.gotoResources()
                }
            } else if (connectionState == QnConnectionManager.Disconnected && currentSessionId == "") {
                loginSessionManager.lastUsedSessionId = ""
                settings.sessionId = ""
                if (stackView.currentItem.objectName != "newConnectionPage")
                    Main.gotoNewSession()
            }
        }

        onConnectionFailed: {
            Main.openFailedSession(
                        currentSessionId,
                        currentHost, currentPort,
                        currentLogin, currentPasswrod,
                        currentSystemName,
                        status, infoParameter)
        }
    }

    Connections {
        target: colorTheme
        onUpdated: QnTheme.loadColors()
    }

    Component.onCompleted: {
        updateNavigationBarPlaceholderSize()
        if (loginSessionManager.lastUsedSessionId) {
            currentSystemName = loginSessionManager.lastUsedSessionSystemName()
            stackView.push(resourcesPageComponent)
            LoginFunctions.connectToServer(
                        loginSessionManager.lastUsedSessionId,
                        loginSessionManager.lastUsedSessionHost(), loginSessionManager.lastUsedSessionPort(),
                        loginSessionManager.lastUsedSessionLogin(), loginSessionManager.lastUsedSessionPassword())
        } else {
            stackView.push([resourcesPageComponent, loginPageComponent])
        }
    }

    function updateNavigationBarPlaceholderSize() {
        if (Qt.platform.os != "android")
            return

        var navBarSize = getNavigationBarHeight()

        if (isPhone && Screen.primaryOrientation == Qt.LandscapeOrientation) {
            navigationBarPlaceholder.realWidth = navBarSize
            navigationBarPlaceholder.realHeight = 0
        } else {
            navigationBarPlaceholder.realWidth = 0
            navigationBarPlaceholder.realHeight = navBarSize
        }
    }

    Screen.onPrimaryOrientationChanged: updateNavigationBarPlaceholderSize()
}
