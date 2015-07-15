import QtQuick 2.4

import com.networkoptix.qml 1.0

QnPopup {
    id: dialog

    overlayLayer: "dialogLayer"
    overlayColor: "#30000000"

    property alias title: title.text

    default property alias data: contentItem.data

    property real maxContentHeight: parent.height - dp(56 * 3)

    function ensureVisible(y1, y2) {
        var cy = flickable.contentY

        if (y2 + flickable.height > flickable.contentHeight)
            cy = flickable.contentHeight - flickable.height

        if (y1 < cy)
            cy = y1

        if (cy < 0)
            cy = 0

        flickable.contentY = cy
    }

    centered: true
    width: dp(56 * 5)
    height: content.height + dp(32)

    Rectangle {
        anchors.fill: parent
        color: QnTheme.dialogBackground
    }

    Column {
        id: content

        width: parent.width - dp(32)

        x: dp(16)
        y: dp(16)

        spacing: dp(16)

        Text {
            id: title
            color: QnTheme.dialogTitleText
            font.weight: Font.DemiBold
            font.pixelSize: sp(20)
            visible: text != ""
        }

        Item {
            id: wrapper

            clip: true

            x: -content.x
            width: dialog.width
            height: Math.min(maxContentHeight, contentItem.height)

            Flickable {
                id: flickable

                x: content.x
                width: content.width
                height: parent.height

                contentWidth: width
                contentHeight: contentItem.height

                flickableDirection: Flickable.VerticalFlick

                Item {
                    id: contentItem
                    width: flickable.width
                    height: childrenRect.height
                }
            }
        }
    }
}
