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

    property real minimumVerticalPadding: 44

    property bool interactive: true

    QtObject
    {
        id: d

        // Content sizes at full scale. If the control is too small, all content is scaled down
        // together, so the icon and the title change size smoothly.
        readonly property real kIconSize: 48
        readonly property real kTitleFontSize: 40
        readonly property real kTitleTopMargin: 16
        readonly property real kTitleMaximumWidth: 300
        readonly property real kMinimumTitleFontSize: 10

        readonly property bool hasTitle: !!control.text

        // Depends only on the control size, so placeholders of the same size look the same
        // for any title text.
        readonly property real contentScale:
        {
            const titleHeight = hasTitle ? kTitleTopMargin + titleMetrics.height : 0
            const referenceHeight = kIconSize + titleHeight + minimumVerticalPadding * 2
            const heightScale = (control.availableHeight - button.requiredHeight) / referenceHeight
            const widthScale = hasTitle ? control.availableWidth / kTitleMaximumWidth : 1
            return MathUtils.bound(0, Math.min(heightScale, widthScale), 1)
        }

        // Prevents division by zero when the control has no size yet.
        readonly property real safeScale: Math.max(contentScale, 0.01)
    }

    FontMetrics
    {
        id: titleMetrics

        font.pixelSize: d.kTitleFontSize
    }

    background: Rectangle
    {
        color: control.backgroundColor

        // The highlight is scaled beyond the bounds, it must not cover the neighbor items.
        clip: true

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
        ColumnLayout
        {
            id: contentLayout

            anchors.centerIn: parent
            spacing: 0

            Item
            {
                Layout.preferredWidth: scaledContent.implicitWidth * d.contentScale
                Layout.preferredHeight: scaledContent.implicitHeight * d.contentScale
                Layout.alignment: Qt.AlignHCenter

                // The content always has full size and is scaled as one item. So when the control
                // is resized (e.g. during tile animation), the icon and the title are not redrawn.
                ColumnLayout
                {
                    id: scaledContent

                    spacing: 0
                    scale: d.contentScale
                    transformOrigin: Item.TopLeft

                    ColoredImage
                    {
                        id: image

                        sourceSize: Qt.size(d.kIconSize, d.kIconSize)
                        primaryColor: foregroundColor

                        Layout.preferredWidth: d.kIconSize
                        Layout.preferredHeight: d.kIconSize
                        Layout.alignment: Qt.AlignHCenter
                    }

                    Text
                    {
                        id: title

                        // One line always fits. More lines are shown only if there is enough
                        // space, otherwise the text is cut with "...".
                        readonly property int fittingLineCount:
                        {
                            const availableHeight =
                                (control.availableHeight - button.requiredHeight) / d.safeScale
                                - d.kIconSize - d.kTitleTopMargin - minimumVerticalPadding * 2
                            return Math.floor(availableHeight / titleMetrics.height)
                        }

                        visible: d.hasTitle
                            && d.kTitleFontSize * d.contentScale >= d.kMinimumTitleFontSize
                        color: control.foregroundColor
                        font.pixelSize: d.kTitleFontSize
                        font.capitalization: Font.AllUppercase
                        wrapMode: Text.WordWrap
                        elide: Text.ElideRight
                        horizontalAlignment: Qt.AlignHCenter
                        maximumLineCount: MathUtils.bound(1, fittingLineCount, 3)

                        Layout.topMargin: d.kTitleTopMargin
                        Layout.preferredWidth: Math.min(
                            d.kTitleMaximumWidth, control.availableWidth / d.safeScale)
                        Layout.alignment: Qt.AlignHCenter
                    }
                }
            }

            Button
            {
                id: button

                readonly property bool hasEnoughSpace: control.availableHeight
                    >= implicitHeight + Layout.topMargin + d.kIconSize + minimumVerticalPadding * 2

                readonly property int requiredHeight:
                    visible ? (implicitHeight + Layout.topMargin) : 0

                visible: !!action && hasEnoughSpace && control.interactive

                Layout.topMargin: 24
                Layout.alignment: Qt.AlignHCenter
            }
        }
    }
}
