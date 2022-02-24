// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.14
import QtQuick.Controls 2.14
import QtQuick.Controls.impl 2.14 as T
import QtQuick.Layouts 1.14

import Nx 1.0

import nx.vms.client.desktop 1.0

AbstractButton
{
    id: button

    property int tab: { return RightPanel.Tab.invalid }
    property bool narrow: false

    property string shortcutHintText

    implicitWidth: narrow ? 56 : 88
    implicitHeight: narrow ? 56 : 84

    checkable: true
    hoverEnabled: true

    icon.color: buttonText.color

    topPadding: narrow ? 8 : 12
    bottomPadding: 8

    background: Rectangle
    {
        id: backgroundRect

        color: button.checked || button.hovered
            ? ColorTheme.colors.dark5
            : ColorTheme.colors.dark4
    }

    contentItem: ColumnLayout
    {
        spacing: 0

        T.IconImage
        {
            id: buttonIcon

            Layout.alignment: Qt.AlignCenter

            name: button.icon.name
            color: button.icon.color
            source: button.icon.source
            sourceSize: Qt.size(32, 32)
        }

        Text
        {
            id: buttonText

            Layout.fillWidth: true
            Layout.fillHeight: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            wrapMode: Text.Wrap
            maximumLineCount: 2
            elide: Text.ElideRight
            visible: !narrow

            font.pixelSize: 12
            font.weight: Font.Medium

            text: button.text

            color: button.checked
                ? ColorTheme.highlight
                : ColorTheme.colors.light10
        }
    }
}
