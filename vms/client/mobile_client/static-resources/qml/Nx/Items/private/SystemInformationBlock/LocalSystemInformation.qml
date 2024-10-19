// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6
import QtQuick.Layouts 1.1
import Nx.Core 1.0

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
                color: enabled ? ColorTheme.colors.light10 : ColorTheme.colors.dark13
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
                color: enabled ? ColorTheme.colors.light10 : ColorTheme.colors.dark13
            }

            visible: user
        }
    }
}
