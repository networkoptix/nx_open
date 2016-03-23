import QtQuick 2.4

import com.networkoptix.qml 1.0

import "../main.js" as Main

Flickable {
    maximumFlickVelocity: Main.isMobile() ? dp(10000) : dp(5000)
    flickDeceleration: Main.isMobile() ? dp(1500) : dp(1500)

    function ensureVisible(x, y, w, h)
    {
        var cx = contentX
        var cy = contentY

        if (x + w > cx + width)
            cx = x + w - width
        if (x < cx)
            cx = x

        if (y + h > cy + height)
            cy = y + h - height
        if (y < cy)
            cy = y

        contentX = cx
        contentY = cy
    }
}
