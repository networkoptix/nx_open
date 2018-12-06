import QtQuick 2.6
import QtQuick.Layouts 1.1
import Nx 1.0

Item
{
    implicitWidth: column.implicitWidth
    implicitHeight: column.implicitHeight

    property alias address: hostText.text
    property alias user: userText.text

    Column
    {
        id: column

        width: parent.width

        topPadding: 6
        leftPadding: -4
        spacing: 2

        RowLayout
        {
            width: parent.width
            spacing: 2

            Image
            {
                source: enabled ? lp("/images/tile_server.png")
                                : lp("/images/tile_server_disabled.png")
            }

            Text
            {
                id: hostText

                Layout.fillWidth: true
                font.pixelSize: 14
                elide: Text.ElideRight
                color: enabled ? ColorTheme.contrast10 : ColorTheme.base13
            }
        }

        RowLayout
        {
            width: parent.width
            spacing: 2

            Image
            {
                source: enabled ? lp("/images/tile_user.png")
                                : lp("/images/tile_user_disabled.png")
            }

            Text
            {
                id: userText

                Layout.fillWidth: true
                font.pixelSize: 14
                elide: Text.ElideRight
                color: enabled ? ColorTheme.contrast10 : ColorTheme.base13
            }

            visible: user
        }
    }
}

