// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Layouts

import Nx.Controls 1.0
import Nx.Core 1.0

Rectangle
{
    id: control

    enum Style { Info, Warning, Error }

    property int style: DialogBanner.Style.Info
    readonly property var styleData:
    {
        switch (style)
        {
            case DialogBanner.Style.Error:
                return {
                    "color": ColorTheme.colors.red_l,
                    "background": ColorTheme.colors.dark10,
                    "icon": "image://skin/20x20/Solid/error.svg"
                }

            case DialogBanner.Style.Warning:
                return {
                    "color": ColorTheme.colors.light4,
                    "background": ColorTheme.colors.dark10,
                    "icon": "image://skin/20x20/Solid/attention.svg"
                }

            case DialogBanner.Style.Info:
            default:
                return {
                    "color": ColorTheme.colors.light4,
                    "background": ColorTheme.colors.dark10,
                    "icon": "image://skin/20x20/Solid/info.svg"
                }
        }
    }

    property alias text: bannerText.text
    property alias buttonText: bannerButton.text
    property alias buttonIcon: bannerButton.icon.source

    property bool closeable: false
    property bool closed: false
    property var watchToReopen

    visible: !(closeable && closed)

    onWatchToReopenChanged:
        closed = false

    signal buttonClicked()
    signal closeClicked()

    color: styleData.background

    implicitHeight: bannerLayout.height + 20

    RowLayout
    {
        id: bannerLayout

        anchors
        {
            left: parent.left
            leftMargin: 16
            top: parent.top
            topMargin: 10
            right: parent.right
            rightMargin: 16
        }

        spacing: 8

        ColoredImage
        {
            id: icon

            Layout.alignment: Qt.AlignTop | Qt.AlignLeft

            sourcePath: control.styleData.icon
            primaryColor: control.styleData.color
            sourceSize: Qt.size(20, 20)
        }

        Column
        {
            spacing: 8

            Layout.alignment: Qt.AlignTop
            Layout.fillWidth: true

            Text
            {
                id: bannerText

                width: parent.width

                topPadding: 2
                wrapMode: Text.WordWrap

                color: control.styleData.color
                font: Qt.font({pixelSize: 14, weight: Font.Medium})
            }

            Button
            {
                id: bannerButton

                visible: !!text
                leftPadding: 6
                rightPadding: 6
                spacing: 4

                textColor: ColorTheme.colors.light4

                background: Rectangle
                {
                    radius: 4
                    color: (!bannerButton.pressed && bannerButton.hovered)
                        ? Qt.rgba(1, 1, 1, 0.12) //< Hovered.
                        : bannerButton.pressed
                            ? Qt.rgba(1, 1, 1, 0.08) //< Pressed.
                            : Qt.rgba(1, 1, 1, 0.1) //< Default.
                }

                onClicked: control.buttonClicked()
            }
        }

        ImageButton
        {
            id: closeButton

            visible: control.closeable

            Layout.alignment: Qt.AlignTop | Qt.AlignRight

            icon.source: "image://skin/20x20/Outline/cross_close.svg"
            icon.color: ColorTheme.colors.light4

            onClicked:
            {
                control.closed = true
                control.closeClicked()
            }
        }
    }
}
