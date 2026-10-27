// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Shapes

import Nx.Core
import Nx.Core.Controls
import Nx.Mobile.Controls

/**
 * A placeholder (overlay) to display in place of a video.
 *
 * Consist of a mandatory image, optional title and optional action button.
 *
 * The title text is wrapped by words and is allowed to occupy maximum three lines; excessive text
 * is elided (replaced with an ellipsis). If the text doesn't fit *horizontally* (in that case
 * the line that doesn't fit consists of just one word), it's proportionally scaled down to fit.
 *
 * If the placeholder doesn't fit *vertically* (including `verticalContentPadding` paddings), then:
 *      - the title hides;
 *      - if there's still not enough space, the action button hides;
 *      - if there's still not enough space, the image and the surrounding `verticalContentPadding`
 *        paddings are proportionally scaled down to fit.
 */
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

    // Extra space around the content. Unlike the control paddings, it is scaled down together with
    // the image if there is not enough space even for the image alone.
    property real verticalContentPadding: 44

    property bool interactive: true

    horizontalPadding: 8
    verticalPadding: 0
    clip: true

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

    contentItem: Item
    {
        id: content

        readonly property real imageSize: 48
        readonly property real titleSpacing: 16
        readonly property real buttonSpacing: 24

        readonly property bool hasButton: !!control.action && control.interactive
        readonly property bool hasTitle: !!control.text

        readonly property real buttonHeight: hasButton ? (buttonSpacing + button.height) : 0
        readonly property real titleHeight:
            hasTitle ? (titleSpacing + title.tightHeight * title.scale) : 0

        readonly property real paddingsHeight: control.verticalContentPadding * 2

        readonly property bool showButton: hasButton
            && (imageSize + buttonHeight + paddingsHeight) <= content.height

        readonly property bool showTitle: hasTitle
            && (imageSize + buttonHeight + titleHeight + paddingsHeight) <= content.height

        Item
        {
            id: contentColumn

            width: content.width
            anchors.verticalCenter: content.verticalCenter

            height:
            {
                let result = image.height //< Might be scaled down if even the image doesn't fit.
                if (content.showTitle)
                    result += content.titleHeight
                if (content.showButton)
                    result += content.buttonHeight
                return result
            }

            ColoredImage
            {
                id: image

                anchors.horizontalCenter: contentColumn.horizontalCenter

                // If there is not enough space even for the image alone, the image and
                // the vertical paddings are scaled down together.
                readonly property real shrinkFactor: MathUtils.bound(0,
                    content.height / (content.imageSize + content.paddingsHeight), 1)

                width: content.imageSize * shrinkFactor
                height: width
                sourceSize: Qt.size(width, height)

                visible: width >= 4
                primaryColor: foregroundColor
            }

            Text
            {
                id: title

                readonly property real tightHeight: lineHeight * lineCount

                width: Math.min(300, content.width)
                scale: MathUtils.bound(0, width / contentWidth, 1)
                transformOrigin: Item.Top

                visible: content.showTitle

                anchors.top: image.bottom
                anchors.topMargin:
                    content.titleSpacing - Math.round((height - tightHeight) * scale / 2)
                anchors.horizontalCenter: contentColumn.horizontalCenter

                font.pixelSize: 40
                font.capitalization: Font.AllUppercase
                color: control.foregroundColor
                horizontalAlignment: Qt.AlignHCenter
                maximumLineCount: 3
                wrapMode: Text.WordWrap

                // A single word must not be elided: it is scaled down to fit instead.
                elide: /\s/.test(text.trim()) ? Text.ElideRight : Text.ElideNone

                // Fixed line height for cross-device stability.
                lineHeightMode: Text.FixedHeight
                lineHeight: 48
            }

            Button
            {
                id: button

                visible: content.showButton
                width: Math.min(content.width, implicitWidth)

                anchors.bottom: contentColumn.bottom
                anchors.horizontalCenter: contentColumn.horizontalCenter
            }
        }
    }
}
