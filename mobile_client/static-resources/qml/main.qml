import QtQuick 2.0
import QtQuick.Window 2.2
import Qt.labs.controls 1.0
import Nx 1.0
import Nx.Controls 1.0
import com.networkoptix.qml 1.0

import "items"
import "controls"

import "main.js" as Main
import "items/QnLoginPage.js" as LoginFunctions

ApplicationWindow
{
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
    property bool connectedToSystem: false

    readonly property bool isPhone: getDeviceIsPhone()

    color: ColorTheme.windowBackground

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

    StackView {
        id: stackView

        width: parent.width - navigationBarPlaceholder.realWidth
        height: parent.height - navigationBarPlaceholder.realHeight
    }

    Item {
        id: overlayBound

        anchors.fill: parent
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

        onConnectionStateChanged:
        {
            var connectionState = connectionManager.connectionState
            if (connectionState == QnConnectionManager.Connected)
            {
                lockScreenOrientation()

                LoginFunctions.saveCurrentSession()
                loginSessionManager.lastUsedSessionId = currentSessionId
                settings.sessionId = currentSessionId
                currentSystemName = connectionManager.systemName

                if (stackView.currentItem.objectName == "newConnectionPage" ||
                    stackView.currentItem.objectName == "loginPage")
                {
                    stackView.setFadeTransition()
                    Main.gotoResources()
                }

                connectedToSystem = true
            }
            else if (connectionState == QnConnectionManager.Disconnected && currentSessionId == "")
            {
                unlockScreenOrientation()

                loginSessionManager.lastUsedSessionId = ""
                settings.sessionId = ""

                if (connectedToSystem || stackView.currentItem.objectName != "loginPage")
                    Main.gotoNewSession()

                connectedToSystem = false
            }
        }

        onConnectionFailed: {
            unlockScreenOrientation()

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
            lockScreenOrientation()
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

    Component.onDestruction:
    {
        connectionManager.disconnectFromServer(true)
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

    function lockScreenOrientation()
    {
        setScreenOrientation(Screen.orientation)
    }

    function unlockScreenOrientation()
    {
        setScreenOrientation(Qt.PrimaryOrientation)
    }
}
