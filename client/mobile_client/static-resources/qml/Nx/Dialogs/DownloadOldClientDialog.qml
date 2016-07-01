import QtQuick 2.6
import Qt.labs.controls 1.0

InformationDialog
{
    id: dialog

    closePolicy: Popup.OnEscape | Popup.OnPressOutside | Popup.OnReleaseOutside
    deleteOnClose: true

    message:
    {
        if (Qt.platform.os == "android")
        {
            return qsTr("To connect to old servers please download the legacy application from Google Play.",
                        "\"Google Play\" is the name of the main Android application store.")
        }
        if (Qt.platform.os == "ios")
        {
            return qsTr("To connect to old servers please download the legacy application from the App Store.",
                        "\"App Store\" is the name of the main Apple application store.")
        }
        return qsTr("To connect to old servers please download the legacy application from the Internet.")
    }

    buttonsModel:
    [
        { "id": "DOWNLOAD", "text": qsTr("Download") },
        "CANCEL"
    ]

    onButtonClicked:
    {
        if (buttonId == "DOWNLOAD")
        {
            var url = applicationInfo.oldMobileClientUrl()
            if (url != "")
            {
                console.log("Opening URL: " + url)
                Qt.openUrlExternally(url)
            }
            else
            {
                console.log("Old client URL is not specified for this platform!")
            }
        }
    }
}
