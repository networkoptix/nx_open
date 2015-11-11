import QtQuick 2.4

import com.networkoptix.qml 1.0

import "../main.js" as Main

ListView {
    maximumFlickVelocity: Main.isMobile() ? dp(10000) : dp(5000)
    flickDeceleration: Main.isMobile() ? dp(1500) : dp(1500)
}
