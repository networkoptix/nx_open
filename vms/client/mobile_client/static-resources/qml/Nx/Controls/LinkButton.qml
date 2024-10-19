// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6
import QtQuick.Controls 2.4
import Nx.Core 1.0

Button
{
    id: control

    property color color: ColorTheme.colors.brand_d2
    property url url

    implicitHeight: 32
    implicitWidth: contentItem ? contentItem.implicitWidth : 0

    background: null
    contentItem: Text
    {
        text: control.text
        width: control.width
        height: control.height
        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: Text.AlignHCenter
        elide: Text.ElideRight
        color: control.color
        font.pixelSize: 15
        font.underline: true
    }

    MouseArea
    {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked:
        {
            if (url != "")
            {
                // Forcing active focus to prevent keyboard capture form the QML side.
                // Otherwise embedded browser can't show keyboard for input fields.
                control.forceActiveFocus()
                openExternalLink(url)
            }

            control.clicked()
        }
    }

    Component.onCompleted: control["activeFocusOnTab"] = false
}
