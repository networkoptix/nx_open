import QtQuick 2.4

import com.networkoptix.qml 1.0

QnPopup {
    id: dialog

    parent: dialogLayer

    property alias title: title.text

    default property alias data: contentItem.data

    property alias footer: footerLoader.sourceComponent

    property real maxContentHeight: parent.height - dp(56 * 3)

    function ensureVisible(y1, y2) {
        var cy = flickable.contentY

        if (cy + flickable.height < y2)
            cy = y2 - flickable.height

        if (y1 < cy)
            cy = y1

        if (cy < 0)
            cy = 0

        flickable.contentY = cy
    }

    implicitWidth: parent ? Math.min(dp(328), parent.width - dp(32)) : dp(56 * 5)
    implicitHeight: content.height + dp(16)
    anchors.centerIn: parent

    Rectangle {
        anchors.fill: parent
        color: QnTheme.dialogBackground
    }

    Column {
        id: content

        width: parent.width
        y: dp(16)

        spacing: dp(16)

        Text {
            id: title
            x: dp(16)
            y: dp(16)
            width: parent.width - dp(32)
            color: QnTheme.dialogTitleText
            font.weight: Font.DemiBold
            font.pixelSize: sp(20)
            elide: Text.ElideRight
            visible: text != ""
        }

        Item {
            id: wrapper

            clip: true

            width: parent.width
            height: Math.min(maxContentHeight, contentItem.height)

            Flickable {
                id: flickable

                width: parent.width
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

            Rectangle {
                width: parent.width
                height: dp(8)

                gradient: Gradient {
                    GradientStop { position: 0.0; color: "#15000000" }
                    GradientStop { position: 1.0; color: "#00000000" }
                }

                opacity: Math.min(height, Math.max(0, flickable.contentY)) / height
            }
        }

        Loader {
            id: footerLoader
            sourceComponent: Component {
                Item {
                    width: 1
                    height: dp(16)
                }
            }
        }
    }

    showAnimation: NumberAnimation {
        target: dialog
        property: "opacity"
        from: 0.0
        to: 1.0
        duration: 100
    }

    hideAnimation: ParallelAnimation {
        ScriptAction {
            script: dialog.parent.hide()
        }
        NumberAnimation {
            target: dialog
            property: "opacity"
            to: 0.0
            duration: 100
        }
    }
}
