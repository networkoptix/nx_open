// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.11

import Nx.Core 1.0

ProgressBar
{
    id: progressBar

    property alias title: progressTitle.text
    property alias percentageFont: percentage.font
    property alias barHeight: groove.height

    property bool showText: true

    font.pixelSize: 12
    font.weight: Font.Medium

    contentItem: ColumnLayout
    {
        spacing: 0

        RowLayout
        {
            id: textRow

            spacing: 8
            visible: progressBar.showText

            Label
            {
                id: progressTitle

                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.alignment: Qt.AlignTop
                horizontalAlignment: Qt.AlignLeft
                color: ColorTheme.colors.brand_core
                elide: Text.ElideRight
                text: "Progress title"
            }

            Label
            {
                id: percentage

                Layout.fillHeight: true
                Layout.alignment: Qt.AlignTop
                Layout.topMargin: progressTitle.baselineOffset - percentage.baselineOffset
                visible: !progressBar.indeterminate
                color: ColorTheme.colors.light4
                text: Math.round(progressBar.position * 100) + "%"
            }
        }

        Item
        {
            id: padding

            visible: textRow.visible
            Layout.fillWidth: true
            height: 3
        }

        Rectangle
        {
            id: groove

            height: 4
            Layout.fillWidth: true
            color: ColorTheme.colors.dark5

            Rectangle
            {
                x: 1
                y: 1
                width: parent.width - 2
                height: parent.height - 2

                color: progressBar.indeterminate
                    ? ColorTheme.colors.brand_d7
                    : "transparent"

                Rectangle
                {
                    id: slider

                    x: normalizedX * parent.width
                    y: 0
                    width: normalizedWidth * parent.width
                    height: parent.height

                    color: progressBar.indeterminate
                        ? ColorTheme.colors.brand_d4
                        : ColorTheme.colors.brand_core

                    readonly property real normalizedX: progressBar.indeterminate
                        ? Math.max(animatedPos, 0.0)
                        : (progressBar.mirrored ? (1 - progressBar.position) : 0)

                    readonly property real normalizedWidth: progressBar.indeterminate
                        ? (1.0 - Math.abs(animatedPos))
                        : progressBar.position

                    property real animatedPos: 0

                    SequentialAnimation on animatedPos
                    {
                        running: progressBar.indeterminate
                        loops: Animation.Infinite

                        NumberAnimation
                        {
                            duration: 750
                            easing.type: Easing.InOutQuad
                            from: progressBar.mirrored ? 1 : -1
                            to: 0
                        }

                        NumberAnimation
                        {
                            duration: 750
                            easing.type: Easing.InOutQuad
                            from: 0
                            to: progressBar.mirrored ? -1 : 1
                        }
                    }
                }
            }
        }

        Rectangle
        {
            id: underline

            height: 1
            Layout.fillWidth: true

            color: progressBar.indeterminate
                ? "transparent"
                : ColorTheme.colors.dark9
        }
    }

    background: null
}
