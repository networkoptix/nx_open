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
    width: 480
    height: 800

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
                toolBar.title = currentItem.title
                toolBar.rightComponent = currentItem.rightToolBarComponent
            }
        }
    }

    Component {
        id: loginPageComponent

        QnLoginPage {
        }
    }

    Component {
        id: resourcesPageComponent

        QnResourcesPage {
            id: resourcesPage

            onVideoRequested: {
                activeResourceId = uuid
                stackView.push(videoPlayerComponent)
            }
        }
    }

    Component {
        id: videoPlayerComponent

        QnVideoPage {
            id: videoPlayerContent
            resourceId: activeResourceId
        }
    }

    Connections {
        target: connectionManager
        onConnected: {
            LoginFunctions.saveCurrentSession()
            Main.gotoResources()
        }
    }

    Connections {
        target: colorTheme
        onUpdated: QnTheme.loadColors()
    }

    Component.onCompleted: {
        stackView.push([resourcesPageComponent, loginPageComponent])
    }
}
