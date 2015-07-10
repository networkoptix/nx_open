import QtQuick 2.4

import com.networkoptix.qml 1.0

QnPopup {
    id: dialog

    overlayLayer: "dialogLayer"
    overlayColor: "#30000000"

    property alias title: title.text

    default property alias data: contentItem.data

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
            id: contentItem
            width: childrenRect.width
            height: childrenRect.height
        }
    }
}
