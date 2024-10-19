// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6
import Nx.Controls 1.0

import Nx.Core 1.0

Rectangle
{
    id: control

    property alias icon: iconButton.icon
    property alias text: textItem.text
    property alias contentColor: textItem.color

    signal clicked

    radius: height / 2
    color: ColorTheme.brightText

    implicitHeight: 36
    implicitWidth: contentRow.width

    Row
    {
        id: contentRow

        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter

        leftPadding: 12

        Text
        {
            id: textItem

            font.pixelSize: 18
            anchors.verticalCenter: parent.verticalCenter
            color: ColorTheme.colors.dark1
        }

        IconButton
        {
            id: iconButton

            height: control.height
            width: height

            onClicked: control.clicked()
            icon.color: control.contentColor
        }
    }
}
