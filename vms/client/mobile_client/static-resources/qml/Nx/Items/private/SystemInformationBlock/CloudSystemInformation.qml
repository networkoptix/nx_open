import QtQuick 2.6
import QtQuick.Layouts 1.1
import Nx 1.0
import com.networkoptix.qml 1.0

Item
{
    implicitWidth: column.implicitWidth
    implicitHeight: column.implicitHeight

    property alias description: descriptionText.text
    readonly property bool cloudAvailable:
        cloudStatusWatcher.status === QnCloudStatusWatcher.Online

    Column
    {
        id: column

        width: parent.width
        spacing: 4

        Text
        {
            id: descriptionText

            width: parent.width
            font.pixelSize: 14
            elide: Text.ElideRight
            color: enabled ? ColorTheme.windowText : ColorTheme.base13
        }

        Image
        {
            x: -2
            source: !cloudAvailable
                ? lp("/images/cloud_unavailable.png")
                : enabled
                    ? lp("/images/cloud.png")
                    : lp("/images/cloud_disabled.png")
        }
    }
}
