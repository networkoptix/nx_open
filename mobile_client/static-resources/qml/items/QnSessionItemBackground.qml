import QtQuick 2.4

import com.networkoptix.qml 1.0

Rectangle {    
    property bool active

    color: active ? QnTheme.sessionItemBackgroundActive : QnTheme.sideNavigationBackground

    Rectangle {
        height: parent.height
        width: dp(3)
        color: QnTheme.sessionItemActiveMark
        visible: active
    }
}

