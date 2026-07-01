// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Shapes

import Nx.Core
import Nx.Core.Controls
import Nx.Mobile.Controls

Control
{
    id: control

    enum Type { Info, Error }

    property int type: VideoPlaceholder.Info
    property alias text: title.text
    property alias imageSource: image.sourcePath
    property alias action: button.action

    property color foregroundColor:
        type === VideoPlaceholder.Error ? ColorTheme.colors.red : ColorTheme.colors.light10

    property color backgroundColor: ColorTheme.colors.dark4

    property color highlightColor:
        type === VideoPlaceholder.Error ? ColorTheme.colors.red : ColorTheme.colors.light16

    property real highlightIntensity: type === VideoPlaceholder.Error ? 0.10 : 0.25

    property real preferredVerticalPadding: 44

    property bool interactive: true

    background: Rectangle
    {
        color: control.backgroundColor

        Shape
        {
            id: highlight

            anchors.fill: parent

            opacity: control.highlightIntensity

            ShapePath
            {
                strokeWidth: -1

                PathRectangle
                {
                    width: highlight.width
                    height: highlight.height
                }

                fillGradient: RadialGradient
                {
                    centerX: highlight.width / 2
                    centerY: highlight.height / 2
                    focalX: centerX
                    focalY: centerY
                    centerRadius: Math.hypot(highlight.width, highlight.height) / 2

                    GradientStop
                    {
                        position: 0.0
                        color: control.highlightColor
                    }

                    GradientStop
                    {
                        position: 0.5
                        color: ColorTheme.transparent(control.highlightColor, 0.4)
                    }

                    GradientStop
                    {
                        position: 1.0
                        color: ColorTheme.transparent(control.highlightColor, 0)
                    }
                }
            }

            transform: Scale
            {
                origin.x: highlight.width / 2
                origin.y: highlight.height / 2
                xScale: 1.4
                yScale: 1.0
            }
        }
    }

    contentItem: ColumnLayout
    {
        id: contentLayout

        spacing: 0

        Item
        {
            Layout.fillHeight: true
            Layout.preferredHeight: preferredVerticalPadding
        }

        ColoredImage
        {
            id: image

            readonly property int requiredHeight:
                Layout.preferredHeight + Layout.topMargin + preferredVerticalPadding * 2

            readonly property bool hasEnoughSpace: control.height >= requiredHeight

            sourceSize: Qt.size(height, height)
            primaryColor: foregroundColor

            Layout.fillHeight: !hasEnoughSpace
            Layout.maximumHeight: 48
            Layout.preferredHeight: 48
            Layout.alignment: Qt.AlignCenter
        }

        Text
        {
            id: title

            readonly property int requiredHeight: text ? (implicitHeight + Layout.topMargin) : 0
            readonly property bool hasEnoughSpace:
                control.height >= (requiredHeight + image.requiredHeight + button.requiredHeight)

            visible: !!text && hasEnoughSpace
            color: control.foregroundColor
            font.pixelSize: 40
            font.capitalization: Font.AllUppercase
            wrapMode: Text.Wrap
            horizontalAlignment: Qt.AlignHCenter
            maximumLineCount: 3

            Layout.topMargin: 16
            Layout.fillWidth: true
            Layout.maximumWidth: 300
            Layout.alignment: Qt.AlignCenter
        }

        Button
        {
            id: button

            readonly property int requiredHeight: action ? (implicitHeight + Layout.topMargin) : 0
            readonly property bool hasEnoughSpace:
                control.height >= (requiredHeight + image.requiredHeight)

            visible: !!action && hasEnoughSpace && control.interactive

            Layout.topMargin: 24
            Layout.alignment: Qt.AlignCenter
        }

        Item
        {
            Layout.fillHeight: true
            Layout.preferredHeight: preferredVerticalPadding
        }
    }
}
