// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Layouts

import Nx.Core
import Nx.Mobile.Controls

import nx.vms.client.core

BaseBottomSheet
{
    id: sheet

    spacing: 24
    topPadding: 40

    component Header: Text
    {
        color: ColorTheme.colors.light4
        textFormat: Text.StyledText
        font.pixelSize: 16
        font.weight: Font.Medium
        lineHeight: 24
        lineHeightMode: Text.FixedHeight
        wrapMode: Text.Wrap

        width: parent.width
    }

    component Step: RowLayout
    {
        property int step
        property alias text: text.text

        width: parent.width

        spacing: 12

        Rectangle
        {
            implicitWidth: 28
            implicitHeight: 28
            radius: width / 2
            color: ColorTheme.colors.light10

            Layout.alignment: Qt.AlignCenter

            Text
            {
                text: step
                anchors.fill: parent
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                font.pixelSize: 14
                font.weight: Font.Medium
                topPadding: 1 //< Position adjustment.
                color: "black"
            }
        }

        Text
        {
            id: text

            color: ColorTheme.colors.light10
            font.pixelSize: 14
            lineHeight: 21
            lineHeightMode: Text.FixedHeight
            wrapMode: Text.Wrap

            verticalAlignment: Text.AlignVCenter
            Layout.fillWidth: true
        }
    }

    Column
    {
        spacing: 16
        width: parent.width

        Header
        {
            text: qsTr("If the site is <font color='%2'>not connected</a> to the %1",
                "%1 is the short cloud name (like 'Cloud')")
                    .arg(Branding.shortCloudName())
                    .arg(ColorTheme.colors.brand)
        }

        Step
        {
            step: 1
            text: qsTr("Log in to the site in %1",
                "%1 is the desktop client name (like 'Nx Witness Client')")
                    .arg(Branding.desktopClientDisplayName())
        }

        Step
        {
            step: 2
            text: qsTr("Click \"Connect Site to %1\" in the %1 tab in Site Administration",
                "%1 is the short cloud name (like 'Cloud')")
                    .arg(Branding.shortCloudName())
        }

        Step
        {
            step: 3
            text: qsTr("Follow the steps of wizard")
        }
    }

    Column
    {
        spacing: 16
        width: parent.width

        Header
        {
            text: qsTr("If the site is <font color='%2'>connected</a> to the %1",
                "%1 is the short cloud name (like 'Cloud')")
                    .arg(Branding.shortCloudName())
                    .arg(ColorTheme.colors.brand)
        }

        Step
        {
            step: 1
            text: qsTr("Connect to %1 by site owner’s account",
                "%1 is the short cloud name (like 'Cloud')")
                    .arg(Branding.shortCloudName())
        }

        Step
        {
            step: 2
            text: qsTr("Open Site Settings page and click “change” link next to the owner's name")
        }

        Step
        {
            step: 3
            text: qsTr("Follow the steps of wizard")
        }
    }

    ButtonBox
    {
        model: [{id: "ok", type: Button.Type.Brand, text: qsTr("OK")}]
        onClicked: sheet.close()
    }
}
