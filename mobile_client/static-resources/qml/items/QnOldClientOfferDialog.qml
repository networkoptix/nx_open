import QtQuick 2.4

import com.networkoptix.qml 1.0

import "../controls"

QnDialog {
    id: dialog

    QtObject {
        id: d

        function downloadOfferString() {
            if (Qt.platform.os == "android") {
                return qsTr("To connect to old servers please download the legacy application from Google Play.",
                            "\"Google Play\" is the name of the main Android application store.")
            } else if (Qt.platform.os == "ios") {
                return qsTr("To connect to old servers please download the legacy application from the App Store.",
                            "\"App Store\" is the name of the main Apple application store.")
            } else {
                return qsTr("To connect to old servers please download the legacy application from the Internet.")
            }
        }
    }

    Text {
        width: dialog.width - dp(32)
        x: dp(16)
        text: d.downloadOfferString()
        font.pixelSize: sp(18)
        wrapMode: Text.WordWrap
        color: QnTheme.dialogText
    }

    footer: Row {
        id: buttonPanel

        width: dialog.width
        height: dp(48)

        QnDialogButton {
            text: qsTr("Download")
            width: parent.width / 2 - dp(0.5)

            onClicked: {
                dialog.hide()
                console.log("opening url: " + applicationInfo.oldMobileClientUrl())
                Qt.openUrlExternally(applicationInfo.oldMobileClientUrl())
            }
        }

        QnDialogButtonSplitter {}

        QnDialogButton {
            text: qsTr("Cancel")
            width: parent.width / 2 - dp(0.5)

            onClicked: dialog.hide()
        }
    }
}
