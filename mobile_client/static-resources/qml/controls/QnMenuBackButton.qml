import QtQuick 2.4
import QtQuick.Controls 1.2
import QtQuick.Controls.Styles 1.2

import "../icons"

Button {
    id: button

    property real progress: 0.0
    property var navigationDrawer

    property bool _menuOpened: false

    style: ButtonStyle {
        background: Item {
            implicitWidth: dp(36)
            implicitHeight: dp(36)
        }

        label: Item {
            QnMenuBackIcon {
                id: icon
                anchors.centerIn: parent
                menuSate: !control._menuOpened
                animationProgress: control.progress
            }
        }
    }

    Behavior on progress {
        id: progressBehavior
        enabled: !navigationDrawer
        NumberAnimation {
            duration: 200
            easing.type: Easing.OutCubic
            running: false
        }
    }

    onNavigationDrawerChanged: {
        if (navigationDrawer)
            progress = Qt.binding(function(){ return navigationDrawer.panelProgress })
        else
            progress = 0.0
    }

    onClicked: {
        if (navigationDrawer)
            navigationDrawer.toggle()
        else
            progress = _menuOpened ? 0.0 : 1.0
    }

    onProgressChanged: {
        if (progress == 0.0)
            _menuOpened = false
        else if (progress == 1.0)
            _menuOpened = true
    }
}
