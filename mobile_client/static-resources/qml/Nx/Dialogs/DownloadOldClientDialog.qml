import QtQuick 2.6
import Qt.labs.controls 1.0

DialogBase
{
    id: dialog

    closePolicy: Popup.OnEscape | Popup.OnPressOutside | Popup.OnReleaseOutside
    deleteOnClose: true

    function _messageString()
    {
        if (Qt.platform.os == "android")
        {
            return qsTr("To connect to old servers please download the legacy application from Google Play.",
                        "\"Google Play\" is the name of the main Android application store.")
        }
        else if (Qt.platform.os == "ios")
        {
            return qsTr("To connect to old servers please download the legacy application from the App Store.",
                        "\"App Store\" is the name of the main Apple application store.")
        }
        else
        {
            return qsTr("To connect to old servers please download the legacy application from the Internet.")
        }
    }

    contentItem: Column
    {
        width: parent.width
        topPadding: 16

        DialogMessage { text: _messageString() }

        Row
        {
            height: 48
            width: parent.width

            DialogButton
            {
                text: qsTr("Download")
                width: parent.width / 2

                onClicked:
                {
                    close()
                    console.log("opening url: " + applicationInfo.oldMobileClientUrl())
                    Qt.openUrlExternally(applicationInfo.oldMobileClientUrl())
                }
            }

            DialogButton
            {
                text: qsTr("Cancel")
                width: parent.width / 2

                onClicked: close()
            }
        }
    }
}
