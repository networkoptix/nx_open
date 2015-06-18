import QtQuick 2.2
import QtQuick.Window 2.0
import QtQuick.Controls 1.2

import com.networkoptix.qml 1.0

import "items"
import "controls"

import "main.js" as Main

Window {
    id: mainWindow

    visible: true
    width: 480
    height: 800

    property string activeResourceId

    Rectangle {
        anchors.fill: parent
        color: QnTheme.windowBackground
    }

    QnToolBar {
        id: toolBar
    }

    StackView {
        id: stackView
        width: parent.width
        anchors.top: toolBar.bottom
        anchors.bottom: parent.bottom

        initialItem: loginPage

        onCurrentItemChanged: {
            if (currentItem) {
                toolBar.title = currentItem.title
                toolBar.leftComponent = currentItem.leftToolBarComponent
                toolBar.rightComponent = currentItem.rightToolBarComponent
            }
        }
    }

    Component {
        id: loginPage

        QnLoginPage {
            onOpenDiscoveredSessionRequested: Main.openDiscoveredSession(host, port, systemName)
        }
    }

    Component {
        id: resourcesPage

        QnResourcesPage {
            onVideoRequested: {
                activeResourceId = uuid
                stackView.push(videoPlayer)
            }
        }
    }

    Component {
        id: videoPlayer

        QnVideoPage {
            id: videoPlayerContent
            resourceId: activeResourceId
        }
    }

    Connections {
        target: connectionManager
        onConnected: {
            stackView.replace(resourcesPage)
        }
    }

    Connections {
        target: colorTheme
        onUpdated: QnTheme.loadColors()
    }
}
