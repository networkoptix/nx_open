import QtQuick 2.4

import com.networkoptix.qml 1.0

import "../main.js" as Main

GridView {
    maximumFlickVelocity: Main.isMobile() ? dp(10000) : dp(5000)
    flickDeceleration: Main.isMobile() ? dp(1500) : dp(1500)

    readonly property int firstVisibleIndex:
    {
        if (contentWidth < cellWidth && contentHeight < cellHeight)
            return -1

        return indexAt(contentX + cellWidth / 2, contentY + cellHeight / 2)
    }

    readonly property int lastVisibleIndex:
    {
        if (contentWidth < cellWidth && contentHeight < cellHeight)
            return -1

        var index = indexAt(contentX + width - cellWidth / 2,
                            contentY + height - cellHeight / 2)
        if (index >= 0)
            return index

        return (firstVisibleIndex >= 0) ? count - 1 : -1
    }
}
