import QtQuick 2.2
import QtQuick.Window 2.0
import QtQuick.Controls 1.2

import com.networkoptix.qml 1.0
import Material 0.1

import "dialogs"
//import "main.js" as Main

ApplicationWindow {
    id: mainWindow

    visible: true
    width: 480
    height: 800

    theme.accentColor: colorTheme.color("nx_base")

    initialPage: QnLoginDialog {

    }

    Connections {
        target: connectionManager
        onDisconnected: {

        }
    }
}
