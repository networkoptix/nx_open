// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

import Nx.Controls
import Nx.Core
import Nx.Core.Controls
import Nx.Core.EventSearch
import Nx.Core.Items
import Nx.Core.Ui
import Nx.Items
import Nx.Mobile
import Nx.Mobile.Controls as MobileControls
import Nx.Mobile.Ui.Sheets
import Nx.Models
import Nx.Screens
import Nx.Settings
import Nx.Ui

import nx.vms.client.core
import nx.vms.client.mobile
import nx.vms.client.mobile.timeline as Timeline

import "items"

Page
{
    id: eventDetailsScreen

    objectName: "eventDetailsScreen"

    property bool isAnalyticsDetails: true
    property QnCameraListModel camerasModel: null
    property alias eventSearchModel: d.accessor.model
    property int currentEventIndex: -1
    property alias backend: backend
    readonly property alias showControls: d.showControls

    signal fullscreenButtonClicked
    signal searchRequested(string text)

    function share()
    {
        if (!backend.isAvailable)
            return

        if (appContext.settings.showHowShareWorksNotification && isAnalyticsDetails)
            howItWorksSheet.open()
        else
            shareBookmarkSheet.showSheet()
    }

    title: qsTr("Preview")
    toolBar.visible: false

    clip: false

    states:
    [
        State
        {
            name: "portrait"
            when: d.portraitOrientation

            AnchorChanges
            {
                target: panel

                anchors.left: preview.left
                anchors.right: preview.right
                anchors.top: preview.bottom
            }

            PropertyChanges
            {
                panelPlaybackControls.visible: true
                previewTimestamp.visible: true
                previewOrientationButton.visible: true

                panel.transparent: false

                playPauseButton.rounded: false
                playPauseButton.icon.width: 24
                playPauseButton.icon.height: 24
                playPauseButton.Layout.preferredWidth: 44
                playPauseButton.Layout.preferredHeight: 44

                previousButton.rounded: false
                previousButton.icon.width: 24
                previousButton.icon.height: 24

                nextButton.rounded: false
                nextButton.icon.width: 24
                nextButton.icon.height: 24

                playbackControls.spacing: 8
            }
        },
        State
        {
            name: "landscape"
            when: !d.portraitOrientation

            AnchorChanges
            {
                target: panel

                anchors.left: preview.left
                anchors.right: preview.right
                anchors.bottom: preview.bottom
            }

            PropertyChanges
            {
                centralPlaybackControls.visible: true
                panelTimestamp.visible: true
                panelOrientationButton.visible: true

                panel.transparent: true

                playPauseButton.rounded: true
                playPauseButton.icon.width: 64
                playPauseButton.icon.height: 64
                playPauseButton.Layout.preferredWidth: 64
                playPauseButton.Layout.preferredHeight: 64

                previousButton.rounded: true
                previousButton.icon.width: 32
                previousButton.icon.height: 32

                nextButton.rounded: true
                nextButton.icon.width: 32
                nextButton.icon.height: 32

                playbackControls.spacing: 44
            }
        }
    ]

    component ControlButton: MobileControls.Button
    {
        property bool rounded: false
        property bool transparent: rounded

        implicitWidth: 44
        implicitHeight: 44

        type: MobileControls.Button.Type.Interface
        icon.width: 24
        icon.height: 24

        radius: rounded ? width / 2 : 6
        backgroundColor: ColorTheme.transparent(parameters.colors[state], transparent ? 0.5 : 1.0)
        foregroundColor: transparent ? ColorTheme.colors.light1 : parameters.textColors[state]
    }

    ShareBookmarkSheet
    {
        id: shareBookmarkSheet

        isAnalyticsItemMode: eventDetailsScreen.isAnalyticsDetails
        backend: ShareBookmarkBackend
        {
            id: backend

            modelIndex: eventSearchModel.index(currentEventIndex, 0)
        }

        onShowHowItWorks: howItWorksSheet.open()
    }

    HowItWorksSheet
    {
        id: howItWorksSheet

        description: qsTr("Sharing opens the new bookmark dialog to generate a playback link" +
            " after setting the sharing options")
        doNotShowAgain: !appContext.settings.showHowShareWorksNotification
        onContinued:
        {
            appContext.settings.showHowShareWorksNotification = !doNotShowAgain
            shareBookmarkSheet.showSheet()
        }
    }

    Rectangle
    {
        color: ColorTheme.colors.dark3
        anchors.fill: preview
    }

    AudioController
    {
        id: audioController

        resource: preview.resource
        serverSessionManager: windowContext.sessionManager
    }

    MediaPlaybackInterruptor
    {
        player: preview.player
        interruptOnInactivity: CoreUtils.isMobilePlatform()
    }

    IntervalPreview
    {
        id: preview

        audioEnabled: audioController.audioEnabled

        width: parent.width
        height:
        {
            if (!d.portraitOrientation)
                return eventDetailsScreen.height

            const realWidth =
                function()
                {
                    if (!preview.isReady)
                        return width / 16 * 9

                    return videoRotation === 0 || videoRotation === 180
                        ? width / videoAspectRatio
                        : width * videoAspectRatio
                }()

            const maxHeight = (eventDetailsScreen.height - panel.height) / 2
            return Math.min(realWidth, maxHeight)
        }

        clip: true
        scalable: true
        hasPreloader: true
        aspectRatio: 0 //< No forced aspect ratio.
        active: true
        autoplay: false
        autoRepeat: false
        color: "transparent"

        onClicked: d.showControls = !d.showControls

        Image
        {
            y: parent.height - height + 1
            width: parent.width
            height: 56

            source: lp("/images/timeline_gradient.png")
            opacity: d.hasControls ? 1 : 0
            visible: d.portraitOrientation && opacity > 0
        }
    }

    VideoDummy
    {
        anchors.fill: preview
        visible: preview.cannotDecryptMedia
        state: "cannotDecryptMedia"
    }

    Flickable
    {
        width: parent.width

        anchors.top: panel.bottom
        anchors.topMargin: 14
        anchors.bottom: parent.bottom

        contentWidth: parent.width
        contentHeight: scrollableData.height

        clip: true

        Column
        {
            id: scrollableData

            width: parent.width - leftPadding - rightPadding
            leftPadding: 15
            rightPadding: 15
            spacing: 14

            Text
            {
                width: parent.width
                elide: Text.ElideRight
                wrapMode: Text.WrapAnywhere
                maximumLineCount: 2

                color: ColorTheme.colors.light16
                font.pixelSize: 16
                font.weight: Font.Medium

                text: d.accessor.getData(currentEventIndex, "resource").name
            }

            MobileControls.TagView
            {
                width: parent.width
                model: d.accessor.getData(currentEventIndex, "tags")
                visible: hasTags
                color: ColorTheme.colors.dark8
            }

            Text
            {
                id: titleText

                width: parent.width

                elide: Text.ElideRight
                wrapMode: Text.WrapAnywhere
                maximumLineCount: 2

                color: ColorTheme.colors.light4
                font.pixelSize: 20
                font.weight: Font.Medium

                text: d.accessor.getData(currentEventIndex, "display")
            }

            Text
            {
                id: descriptionText

                width: parent.width
                color: ColorTheme.colors.light10
                font.pixelSize: 14
                font.weight: Font.Normal
                wrapMode: Text.Wrap
                text: d.accessor.getData(currentEventIndex, "description")
                visible: !!text

                onLinkActivated: (link) =>
                {
                    Workflow.openDialog("qrc:/qml/Nx/Web/LinkAboutToOpenDialog.qml",
                        {"link": link});
                }
            }

            AnalyticsAttributeTable
            {
                id: attributeTable

                width: parent.width

                attributes: d.accessor.getData(currentEventIndex, "analyticsAttributes") ?? []

                interactive: true
                contextMenu: searchMenu
                highlight.visible: false
                nameFont.pixelSize: 14
                nameColor: ColorTheme.colors.light10
                valueFont.pixelSize: 14
                valueColor: ColorTheme.colors.light6
                valueFont.weight: Font.Medium
                valueAlignment: Text.AlignRight
                colorBoxSize: 18
                rowSpacing: 16
                separatorsVisible: true
            }
        }
    }

    Menu
    {
        id: searchMenu

        property var attribute: null
        property int margin: 20

        x: margin
        y: parent.height - height - margin
        width: parent.width - x - margin

        function popup()
        {
            attribute = attributeTable.hoveredItem
            open() //< Open at the current position.
        }

        MenuItem
        {
            text: searchMenu.attribute?.displayedValues.join(", ") ?? ""
            horizontalAlignment: Qt.AlignHCenter
            textColor: ColorTheme.colors.light14
        }

        MenuItem
        {
            text: qsTr("Search by attribute")
            horizontalAlignment: Qt.AlignHCenter

            onClicked:
            {
                eventDetailsScreen.searchRequested(EventSearchHelpers.createSearchRequestText(
                    searchMenu.attribute.id, searchMenu.attribute.values))
            }
        }

        MenuItem
        {
            text: qsTr("Cancel")
            horizontalAlignment: Qt.AlignHCenter
        }
    }

    LayoutItemProxy
    {
        id: centralPlaybackControls

        anchors.centerIn: preview

        target: playbackControls
        opacity: d.hasControls && !preview.cannotDecryptMedia ? 1.0 : 0.0
        enabled: opacity > 0
    }

    LayoutItemProxy
    {
        id: previewTimestamp

        anchors.left: preview.left
        anchors.bottom: preview.bottom
        anchors.margins: 20

        target: timestampText
    }

    LayoutItemProxy
    {
        id: previewOrientationButton

        anchors.right: preview.right
        anchors.bottom: preview.bottom
        anchors.rightMargin: 8
        anchors.bottomMargin: 10

        target: orientationModeButton
    }

    Rectangle
    {
        id: panel

        property bool transparent: false
        color: transparent ? "transparent" : ColorTheme.colors.dark6

        visible: d.hasControls
        height: 68

        RowLayout
        {
            id: panelLayout

            anchors.fill: parent
            anchors.margins: 12

            spacing: 8

            LayoutItemProxy
            {
                id: panelTimestamp

                target: timestampText
                Layout.fillWidth: true
                Layout.preferredWidth: panel.width
            }

            ControlButton
            {
                id: repeatButton

                checkable: true
                checked: preview.autoRepeat
                transparent: panel.transparent
                visible: !preview.cannotDecryptMedia
                icon.source: "image://skin/24x24/Solid/repeat.svg"
                Layout.fillWidth: true
                Layout.minimumWidth: implicitWidth

                onClicked:
                {
                    preview.autoRepeat = checked
                    preview.autoplay = checked
                }
            }

            LayoutItemProxy
            {
                id: panelPlaybackControls

                target: playbackControls
                Layout.fillWidth: true
                Layout.minimumWidth: implicitWidth
            }

            ControlButton
            {
                id: goToCameraButton

                visible: !preview.cannotDecryptMedia
                transparent: panel.transparent
                icon.source: "image://skin/24x24/Outline/show_on_layout.svg"
                Layout.fillWidth: true
                Layout.minimumWidth: implicitWidth

                onClicked: d.goToCamera()
            }

            LayoutItemProxy
            {
                id: panelOrientationButton

                target: orientationModeButton
                Layout.fillWidth: true
                Layout.minimumWidth: implicitWidth
            }
        }
    }

    RowLayout
    {
        id: playbackControls

        spacing: 8

        ControlButton
        {
            id: previousButton

            icon.source: "image://skin/24x24/Outline/chunk_previous.svg"
            enabled: currentEventIndex < d.accessor.count - 1

            Layout.fillWidth: true

            onClicked: ++currentEventIndex
        }

        ControlButton
        {
            id: playPauseButton

            icon.source: preview.playingPlayerState && !preview.cannotDecryptMedia
                ? "image://skin/24x24/Outline/pause.svg"
                : "image://skin/24x24/Outline/play_small.svg"
            enabled: !preview.cannotDecryptMedia

            Layout.fillWidth: true

            onClicked:
            {
                if (preview.playingPlayerState)
                {
                    preview.preview()
                    return
                }

                preview.play(slider.value == slider.to
                    ? preview.startTimeMs
                    : preview.position)
            }
        }

        ControlButton
        {
            id: nextButton

            icon.source: "image://skin/24x24/Outline/chunk_future.svg"
            enabled: currentEventIndex > 0

            Layout.fillWidth: true

            onClicked: --currentEventIndex
        }
    }

    ControlButton
    {
        id: orientationModeButton

        width: 48
        height: 48
        opacity: !preview.cannotDecryptMedia ? 1.0 : 0.0
        enabled: opacity > 0
        transparent: true

        icon.source: d.portraitOrientation
            ? "image://skin/24x24/Outline/fullscreen_view_mode.svg"
            : "image://skin/24x24/Outline/exit_fullscreen_mode.svg"
        icon.width: 24
        icon.height: 24

        onClicked: eventDetailsScreen.fullscreenButtonClicked()
    }

    Text
    {
        id: timestampText

        font.pixelSize: 16
        font.weight: Font.Medium
        color: ColorTheme.colors.light4
        opacity: !preview.cannotDecryptMedia ? 1.0 : 0.0

        text: EventSearchUtils.timestampText(slider.value, windowContext.mainSystemContext)
    }

    Rectangle
    {
        id: bottomNavigationArea

        y: parent.height
        x: -windowParams.leftMargin
        height: windowParams.bottomMargin
        width: parent.width + windowParams.leftMargin + windowParams.rightMargin
        color: ColorTheme.colors.dark1
    }

    Slider
    {
        id: slider

        property bool playerWasPlaying: false

        visible: opacity > 0 && !preview.cannotDecryptMedia
        opacity: d.hasControls ? 1 : 0

        x: panel.x
        y: panel.y - slider.height + (slider.height - sliderBackground.height) / 2
        width: panel.width
        height: 36
        leftPadding: 12
        rightPadding: 12

        from: preview.startTimeMs
        to: preview.startTimeMs + preview.durationMs

        handle: Rectangle
        {
            id: handle
            width: 24
            height: 24

            x: slider.visualPosition * (slider.width - slider.leftPadding - slider.rightPadding)
                + slider.leftPadding - handle.width / 2
            anchors.verticalCenter: slider.verticalCenter

            radius: 12
            color: ColorTheme.colors.light10
        }

        background: Rectangle
        {
            id: sliderBackground

            width: parent.width
            height: 4
            color: ColorTheme.colors.light17
            anchors.verticalCenter: slider.verticalCenter
        }

        onValueChanged:
        {
            if (!pressed && value == to)
                preview.preview()
        }

        onPressedChanged:
        {
            if (pressed)
            {
                playerWasPlaying = preview.playingPlayerState
                preview.preview()
            }
            else if (playerWasPlaying && value != to)
            {
                CoreUtils.executeLater(function()
                {
                    preview.play(preview.position)
                }, this)
            }
        }

        Connections
        {
            target: slider
            enabled: slider.pressed

            function onValueChanged()
            {
                preview.setPosition(slider.value)
            }
        }

        Binding
        {
            target: slider
            when: !slider.pressed
            property: "value"
            value: preview.atEnd ? slider.to : preview.position
        }
    }

    onEventSearchModelChanged: d.updateCurrentEvent()
    onCurrentEventIndexChanged: d.updateCurrentEvent()

    Binding
    {
        target: d
        property: "hasControls"
        value: d.portraitOrientation || d.showControls
    }

    QtObject
    {
        id: d

        readonly property bool portraitOrientation: eventDetailsScreen.width < eventDetailsScreen.height
        onPortraitOrientationChanged: d.hasControls = true

        property bool hasControls: true

        property ModelDataAccessor accessor: ModelDataAccessor {}
        property bool showControls: true
        property int exclusionAreaY: eventDetailsScreen.activePage
            ? slider.parent.mapToGlobal(0, slider.y).y * Screen.devicePixelRatio
            : 0

        function updateCurrentEvent()
        {
            if (!eventSearchModel || currentEventIndex < 0)
            {
                preview.resource = null
                preview.startTimeMs = 0
                preview.durationMs = 0
                preview.setPosition(0)
                return
            }

            preview.stop()

            const resource = accessor.getData(currentEventIndex, "resource")
            if (!resource)
                return;

            preview.resource = resource

            const paddingTimeMs = eventDetailsScreen.isAnalyticsDetails
                ? CoreSettings.iniConfigValue("previewPaddingTimeMs")
                : 0
            preview.startTimeMs =
                accessor.getData(currentEventIndex, "timestampMs") - paddingTimeMs
            preview.durationMs =
                accessor.getData(currentEventIndex, "durationMs") + paddingTimeMs * 2
            preview.setPosition(preview.startTimeMs)
            slider.value = preview.startTimeMs

            preview.play(preview.startTimeMs) //< Loads the stream anyway.
        }

        function updateStatusBarVisibility()
        {
            if (d.hasControls)
                windowContext.ui.windowHelpers.exitFullscreen()
            else
                windowContext.ui.windowHelpers.enterFullscreen()
        }

        function goToCamera()
        {
            Workflow.openVideoScreen(preview.resource, undefined, slider.from, camerasModel,
                eventDetailsScreen.isAnalyticsDetails
                    ? Timeline.ObjectsLoader.ObjectsType.analytics
                    : Timeline.ObjectsLoader.ObjectsType.bookmarks)
        }

        function updateGestureExclusionArea()
        {
            windowContext.ui.windowHelpers.setGestureExclusionArea(
                d.exclusionAreaY, slider.height * Screen.devicePixelRatio)
        }

        onHasControlsChanged: updateStatusBarVisibility()

        onExclusionAreaYChanged: updateGestureExclusionArea()
    }

    Component.onCompleted:
    {
        d.updateStatusBarVisibility()
        d.updateGestureExclusionArea()
    }
}
