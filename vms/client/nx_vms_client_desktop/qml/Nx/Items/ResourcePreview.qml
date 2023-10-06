// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15
import Qt5Compat.GraphicalEffects

import Nx 1.0
import Nx.Core 1.0
import Nx.Controls 1.0

import nx.vms.client.core 1.0

Item
{
    id: preview

    /** Preview image source. */
    property AbstractResourceThumbnail source: null

    /**
     * Image containter. Its geometry equals to the image geometry.
     * If some kind of overlay is required, it should be parented in it.
     */
    readonly property alias imageArea: overlayHolder

    /** Thumbnail loading status. */
    readonly property alias status: content.status

    /** Whether to apply default rotation (`source.rotationQuadrants`) to the preview image. */
    property bool applyRotation: true

    /** No data text. */
    property string noDataText: qsTr("NO DATA")

    /** Text font pixel size and weight. */
    property int textSize: 13
    property int fontWeight: Font.Normal

    /** No data text and loading indicator foreground color. */
    property color foregroundColor: ColorTheme.windowText

    /** Loading indicator dot diameter and spacing. */
    property real loadingIndicatorDotSize: 8
    property real loadingIndicatorDotSpacing: loadingIndicatorDotSize

    property int loadingIndicatorTimeoutMs: 5000

    property bool blurImageWhileUpdating: false

    /** Thumbnail aspect ratio, without rotation. */
    readonly property alias sourceAspectRatio: content.sourceAspectRatio

    /** Image area aspect ratio, with possible rotation. */
    readonly property alias effectiveAspectRatio: content.aspectRatio

    /** Whether obsolete previews are dimmed and marked with text. */
    property bool markObsolete: false

    /** Obsolete preview overlay text. */
    property string obsoletePreviewText: qsTr("OUTDATED")

    /** No access preview overlay text. */
    property string noAccessText: qsTr("NO ACCESS")

    /** Obsolete preview marker background and text colors. */
    property color obsolescenceDimmerColor: ColorTheme.transparent(ColorTheme.colors.dark5, 0.3)
    property color obsolescenceTextColor: ColorTheme.light

    /** Default implicit size. */
    implicitWidth: 190.0 * Math.sqrt(effectiveAspectRatio)
    implicitHeight: 190.0 / Math.sqrt(effectiveAspectRatio)

    Item
    {
        id: content

        anchors.centerIn: parent
        width: previewSize.width
        height: previewSize.height
        clip: true

        readonly property int status: preview.source
            ? preview.source.status
            : AbstractResourceThumbnail.Status.empty

        readonly property bool hasAccess: accessHelper.canViewLive

        readonly property int rotationQuadrants: (preview.applyRotation && preview.source)
            ? preview.source.rotationQuadrants
            : 0

        readonly property bool dimensionsSwapped: (rotationQuadrants % 2) != 0

        readonly property real sourceAspectRatio: (preview.source && preview.source.aspectRatio)
            ? preview.source.aspectRatio
            : (16.0 / 9.0)

        readonly property real aspectRatio: dimensionsSwapped
            ? (1.0 / sourceAspectRatio)
            : sourceAspectRatio

        readonly property size previewSize: (aspectRatio > (preview.width / preview.height))
            ? Qt.size(preview.width, preview.width / aspectRatio)
            : Qt.size(preview.height * aspectRatio, preview.height)

        readonly property bool loadingIndicatorRequired: image.status != Image.Ready
            && (status == AbstractResourceThumbnail.Status.loading
                || status == AbstractResourceThumbnail.Status.empty)

        AccessHelper
        {
            id: accessHelper
            resource: (preview.source && preview.source.resource) || null
        }

        Item
        {
            id: imageHolder

            anchors.centerIn: content
            width: content.dimensionsSwapped ? content.height : content.width
            height: content.dimensionsSwapped ? content.width : content.height
            rotation: content.rotationQuadrants * 90.0

            readonly property bool blurRequired: blurImageWhileUpdating
                && image.status === Image.Ready
                && content.status === AbstractResourceThumbnail.Status.loading

            FastBlur
            {
                id: imageBlur

                anchors.fill: blurContainer
                source: blurContainer
                radius: imageHolder.blurRequired ? 32 : 0
                z: 1

                visible: imageHolder.blurRequired

                Behavior on radius
                {
                    NumberAnimation
                    {
                        duration: 250
                        easing.type: Easing.InCubic
                    }
                }
            }

            // Blur the blurContainer instead of the image to avoid a crash in Qt6.
            Item
            {
                id: blurContainer

                anchors.fill: imageHolder

                Image
                {
                    id: image

                    anchors.fill: parent
                    fillMode: Image.Stretch
                    source: (preview.source && preview.source.url) || ""
                    cache: false

                    visible: !!source.toString()
                        && !imageHolder.blurRequired
                        && content.status !== AbstractResourceThumbnail.Status.unavailable
                }
            }
        }

        Rectangle
        {
            id: obsolescenceMarker

            anchors.fill: content
            color: preview.obsolescenceDimmerColor
            visible: image.visible && preview.markObsolete && preview.source
                && preview.source.obsolete && !!preview.source.url

            Text
            {
                id: obsolescenceText

                anchors.fill: obsolescenceMarker
                verticalAlignment: Qt.AlignVCenter
                horizontalAlignment: Qt.AlignHCenter

                font.pixelSize: preview.textSize
                font.weight: preview.fontWeight
                fontSizeMode: Text.Fit
                minimumPixelSize: 6
                leftPadding: content.width / 10
                rightPadding: leftPadding
                topPadding: content.height / 10
                bottomPadding: topPadding

                elide: Text.ElideRight //< To ensure `truncated` is true when text does not fit.
                visible: !truncated

                color: preview.obsolescenceTextColor
                text: preview.obsoletePreviewText
            }
        }

        Text
        {
            id: noData

            anchors.fill: content
            verticalAlignment: Qt.AlignVCenter
            horizontalAlignment: Qt.AlignHCenter

            font.pixelSize: accessHelper.canViewLive ? preview.textSize : 32
            font.weight: preview.fontWeight
            fontSizeMode: Text.Fit
            minimumPixelSize: 6
            leftPadding: content.width / 10
            rightPadding: leftPadding
            topPadding: content.height / 10
            bottomPadding: topPadding

            elide: Text.ElideRight //< To ensure `truncated` is true when text does not fit.
            visible: !truncated && (content.status == AbstractResourceThumbnail.Status.unavailable
                || loadingIndicator.timedOut)

            color: accessHelper.canViewLive ? preview.foregroundColor : ColorTheme.colors.red_core
            text: accessHelper.canViewLive ? preview.noDataText : preview.noAccessText
        }

        ScaleAdjuster
        {
            anchors.centerIn: content

            resizeToContent: true
            maximumWidth: content.width / 1.5
            maximumHeight: content.height / 2

            contentItem: NxDotPreloader
            {
                id: loadingIndicator

                color: preview.foregroundColor
                spacing: preview.loadingIndicatorDotSpacing
                dotRadius: preview.loadingIndicatorDotSize / 2

                property bool timedOut: false
                property bool shown: content.loadingIndicatorRequired && !timedOut

                Timer
                {
                    id: indicatorTimeoutTimer
                    interval: preview.loadingIndicatorTimeoutMs

                    onTriggered:
                        loadingIndicator.timedOut = true

                    function handleStateChange()
                    {
                        stop()
                        loadingIndicator.timedOut = false
                        if (content.visible && content.loadingIndicatorRequired && interval > 0)
                            start()
                    }
                }

                Connections
                {
                    target: content

                    function onVisibleChanged()
                    {
                        indicatorTimeoutTimer.handleStateChange()
                    }

                    function onLoadingIndicatorRequiredChanged()
                    {
                        indicatorTimeoutTimer.handleStateChange()
                    }

                    function onStatusChanged() //< To restart at loading -> empty transition.
                    {
                        indicatorTimeoutTimer.handleStateChange()
                    }
                }

                Connections
                {
                    target: preview

                    function onSourceChanged()
                    {
                        indicatorTimeoutTimer.handleStateChange()
                    }
                }
            }

            opacity: loadingIndicator.shown ? 1.0 : 0.0
            visible: opacity > 0.0

            Behavior on opacity
            {
                NumberAnimation
                {
                    duration: loadingIndicator.shown ? 200 : 700
                    easing.type: Easing.InCubic
                }
            }
        }

        Item
        {
            id: overlayHolder
            anchors.fill: content
        }
    }
}
