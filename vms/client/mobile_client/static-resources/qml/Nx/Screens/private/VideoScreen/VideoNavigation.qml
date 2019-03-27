import QtQuick 2.6
import QtGraphicalEffects 1.0
import QtQuick.Window 2.2
import QtQuick.Controls 2.4
import Nx 1.0
import Nx.Media 1.0
import Nx.Controls 1.0
import com.networkoptix.qml 1.0
import nx.client.mobile 1.0

// TODO: #ynikitenkov After 18.1 refactor this control.

Item
{
    id: videoNavigation

    readonly property bool hasArchive: d.hasArchive && videoNavigation.canViewArchive
    property var videoScreenController
    property bool paused: true
    property bool ptzAvailable: false
    property real controlsOpacity: 1.0
    property alias animatePlaybackControls: playbackControlsOpacityBehaviour.enabled
    property bool canViewArchive: true
    property int buttonsPanelHeight: buttonsPanel.visible ? buttonsPanel.height : 0

    property alias motionSearchMode: motionSearchModeButton.checked
    property alias motionFilter: cameraChunkProvider.motionFilter
    property alias changingMotionRoi: timeline.changingMotionRoi
    property bool hasCustomRoi: false;
    property bool drawingRoi: false
    property string warningText

    signal ptzButtonClicked()
    signal switchToNextCamera()
    signal switchToPreviousCamera()

    implicitWidth: parent ? parent.width : 0
    implicitHeight: navigator.height + buttonsPanel.height
    anchors.bottom: parent ? parent.bottom : undefined

    onDrawingRoiChanged: updateWarningTextTimer.restart()
    onHasCustomRoiChanged: updateWarningTextTimer.restart()
    onMotionSearchModeChanged: updateWarningTextTimer.restart()

    Connections
    {
        target: videoScreenController
        onResourceIdChanged:
        {
            actionButtonsPanelOpacityBehaviour.enabled = false
            actionButtonsPanel.opacity = 0
            actionButtonsPanelOpacityBehaviour.enabled = true
            if (d.windowSize > 0)
                timeline.windowSize = d.lastWindowSize
        }
    }

    Connections
    {
        target: videoScreenController.mediaPlayer

        onPlaybackStateChanged:
        {
            var state = videoScreenController.mediaPlayer.playbackState
            if (state == MediaPlayer.Previewing)
                return //< In case of previewing we do not change paused state.

            videoNavigation.paused = state != MediaPlayer.Playing
        }
    }

    Object
    {
        id: d

        readonly property bool loadingChunks:
            cameraChunkProvider.loading || cameraChunkProvider.loadingMotion
        property bool loaded: videoScreenController.mediaPlayer.mediaStatus === MediaPlayer.Loaded
        property bool controlsNeeded: !cameraChunkProvider.loading || loaded

        property real controlsOpacity:
            Math.min(videoNavigation.controlsOpacity, controlsOpacityInternal)

        property real lastWindowSize: -1
        property real controlsOpacityInternal: controlsNeeded ? 1.0 : 0.0
        Behavior on controlsOpacityInternal
        {
            id: playbackControlsOpacityBehaviour

            NumberAnimation { duration: 200 }
        }

        property real timelineOpacity: cameraChunkProvider.loading ? 0.0 : 1.0
        Behavior on timelineOpacity
        {
            NumberAnimation { duration: d.timelineOpacity > 0 ? 0 : 200 }
        }

        onLoadingChunksChanged: updateWarningTextTimer.restart()
        onLoadedChanged: d.updateTimelinePosition()

        Connections
        {
            target: videoScreenController.mediaPlayer
            onPositionChanged: d.updateTimelinePosition()
        }

        function goToPosition(position, savePosition)
        {
            resumePosition = -1
            timeline.timelineView.position = position
            videoScreenController.setPosition(position, savePosition)
        }

        function updateTimelinePosition()
        {
            if (videoScreenController.mediaPlayer.mediaStatus !== MediaPlayer.Loaded)
                return

            if (!timeline.moving && !d.liveMode)
            {
                timeline.autoReturnToBounds = false
                timeline.position = videoScreenController.mediaPlayer.position
            }
        }

        Timer
        {
            // Updates warning message after some delay to prevent fast changes.
            id: updateWarningTextTimer

            interval: 10
            repeat: false
            onTriggered: d.updateWarningText()
        }

        function updateWarningText()
        {
            if (loadingChunks)
                return

            var motionMode = videoNavigation.motionSearchMode
            var hasMotionChunks = cameraChunkProvider.hasMotionChunks()
            if (!motionMode || hasMotionChunks || videoNavigation.drawingRoi)
                videoNavigation.warningText = ""
            else if (!cameraChunkProvider.hasChunks())
                videoNavigation.warningText = qsTr("No motion data for this camera")
            else if (videoNavigation.hasCustomRoi)
                videoNavigation.warningText = qsTr("No motion found in the selected area")
            else
                videoNavigation.warningText = qsTr("No motion found in the visible area")
        }

        readonly property bool hasArchive: timeline.startBound > 0
        readonly property bool liveMode:
            videoScreenController
            && videoScreenController.mediaPlayer.liveMode
            && !playbackController.paused
        property real resumePosition: -1

        function updateNavigatorPosition()
        {
            if (Screen.primaryOrientation === Qt.PortraitOrientation)
            {
                navigator.y = 0
                navigatorMouseArea.drag.target = undefined
            }
            else
            {
                navigatorMouseArea.drag.target = navigator
            }
        }
        Screen.onPrimaryOrientationChanged: updateNavigatorPosition()

        readonly property var locale: Qt.locale()
    }

    QnCameraChunkProvider
    {
        id: cameraChunkProvider

        resourceId: videoScreenController.resourceId

        onLoadingChanged:
        {
            if (loading)
                return

            var liveMs = (new Date()).getTime()
            var lastChunkEndMs = closestChunkEndMs(liveMs, false)

            if (lastChunkEndMs <= 0)
                lastChunkEndMs = liveMs

            if (d.lastWindowSize > 0)
                return

            timeline.windowSize = Math.max((liveMs - lastChunkEndMs) / 0.4, timeline.defaultWindowSize)
        }
    }

    Timer
    {
        interval: 30000
        triggeredOnStart: true
        running: true
        repeat: true
        onTriggered: cameraChunkProvider.update()
    }

    Button
    {
        y: 56 / 2 - height / 2
        padding: 8
        labelPadding: 0
        width: 56
        height: width
        color: ColorTheme.transparent(ColorTheme.base1, 0.2)
        icon.source: lp("/images/previous.png")
        radius: width / 2
        z: 1
        onClicked: videoNavigation.switchToPreviousCamera()
    }

    Button
    {
        y: 56 / 2 - height / 2
        anchors.right: parent.right
        padding: 8
        labelPadding: 0
        width: 56
        height: width
        color: ColorTheme.transparent(ColorTheme.base1, 0.2)
        icon.source: lp("/images/next.png")
        radius: width / 2
        z: 1
        onClicked: videoNavigation.switchToNextCamera()
    }

    Item
    {
        id: navigator

        implicitWidth: parent.width
        implicitHeight: videoNavigation.canViewArchive
            ? timeline.height + playbackController.height - 16
            : 56

        Behavior on y { SmoothedAnimation { duration: 200; reversingMode: SmoothedAnimation.Sync } }

        MouseArea
        {
            id: navigatorMouseArea

            anchors.fill: navigator
            drag.axis: Drag.YAxis
            drag.minimumY: 0
            drag.maximumY: buttonsPanel.height
            drag.filterChildren: true
            drag.threshold: 10

            property real prevY

            onPressed:
            {
                /* We propagate composed events for areas free of the UI controls (timeline, play/pause button). */
                propagateComposedEvents =
                        (mouse.y < height - timeline.height) &&
                        (mouse.x < playbackController.x || mouse.x > playbackController.x + playbackController.width)

                if (drag.target)
                    prevY = drag.target.y
            }
            onMouseYChanged:
            {
                if (drag.target)
                    prevY = drag.target.y
            }
            onReleased:
            {
                if (!drag.target)
                    return

                var dir = drag.target.y - prevY

                if (dir > 1)
                {
                    drag.target.y = drag.maximumY
                }
                else if (dir < -1)
                {
                    drag.target.y = drag.minimumY
                }
                else
                {
                    var mid = (drag.minimumY + drag.maximumY) / 2
                    if (drag.target.y < mid)
                        drag.target.y = drag.minimumY
                    else
                        drag.target.y = drag.maximumY
                }
            }
        }

        MotionPlaybackMaskWatcher
        {
            active: videoNavigation.motionSearchMode
            mediaPlayer: videoScreenController.mediaPlayer
            chunkProvider: cameraChunkProvider
        }

        Timeline
        {
            id: timeline

            property bool resumeWhenDragFinished: false

            motionSearchMode: videoNavigation.motionSearchMode
            serverTimeZoneShift: videoScreenController.resourceHelper.serverTimeOffset;
            enabled: d.hasArchive
            visible: videoNavigation.canViewArchive

            anchors.bottom: parent.bottom

            x: mainWindow.hasNavigationBar ? 0 : -mainWindow.leftPadding
            width: mainWindow.hasNavigationBar ? parent.width : mainWindow.width

            height: 96

            stickToEnd: d.liveMode && !paused

            chunkBarHeight: 16
            textY: height - chunkBarHeight - 16 - 14

            chunkProvider: cameraChunkProvider
            startBound: cameraChunkProvider.bottomBound

            readonly property color lineColor: ColorTheme.transparent(ColorTheme.base1, 0.2)
            readonly property real lineOpacity:
                videoNavigation.canViewArchive && d.hasArchive ? d.timelineOpacity : 0

            Rectangle
            {
                width: parent.width
                height: 1
                anchors.bottom: timeline.bottom
                anchors.bottomMargin: -1
                opacity: timeline.lineOpacity
                color: timeline.lineColor
            }

            Rectangle
            {
                width: parent.width
                height: 1
                anchors.bottom: timeline.bottom
                anchors.bottomMargin: timeline.chunkBarHeight
                opacity: timeline.lineOpacity
                color: timeline.lineColor
            }

            onWindowSizeChanged: d.lastWindowSize = windowSize

            onMovingChanged:
            {
                if (!moving)
                {
                    videoScreenController.setPosition(position, true)
                    if (resumeWhenDragFinished)
                        videoScreenController.play()
                    else
                        videoScreenController.pause()
                    timeline.autoReturnToBounds = true
                }
            }

            onPositionTapped: d.goToPosition(position, true)

            onPositionChanged:
            {
                if (!dragging)
                    return

                videoScreenController.setPosition(position)
            }

            onDraggingChanged:
            {
                if (dragging)
                {
                    resumeWhenDragFinished = !videoNavigation.paused
                    videoScreenController.preview()
                    d.resumePosition = -1
                }
            }
        }

        Item
        {
            id: timelineMask

            anchors.fill: timeline
            visible: false

            Rectangle
            {
                id: blurRectangle

                readonly property real blurWidth: 16
                readonly property real margin: 8

                width: timeline.height - timeline.chunkBarHeight
                height: timeline.width
                rotation: 90
                anchors.centerIn: parent
                anchors.verticalCenterOffset: -timeline.chunkBarHeight / 2
                gradient: Gradient
                {
                    GradientStop { position: 0.0; color: Qt.rgba(1.0, 1.0, 1.0, 1.0) }
                    GradientStop { position: (timeLiveLabel.x - blurRectangle.blurWidth - blurRectangle.margin) / timeline.width; color: Qt.rgba(1.0, 1.0, 1.0, 1.0) }
                    GradientStop { position: (timeLiveLabel.x - blurRectangle.margin) / timeline.width; color: Qt.rgba(1.0, 1.0, 1.0, 0.0) }
                    GradientStop { position: 0.5; color: Qt.rgba(1.0, 1.0, 1.0, 0.0) }
                    GradientStop { position: (timeLiveLabel.x + timeLiveLabel.width + blurRectangle.margin) / timeline.width; color: Qt.rgba(1.0, 1.0, 1.0, 0.0) }
                    GradientStop { position: (timeLiveLabel.x + timeLiveLabel.width + blurRectangle.blurWidth + blurRectangle.margin) / timeline.width; color: Qt.rgba(1.0, 1.0, 1.0, 1.0) }
                    GradientStop { position: 1.0; color: Qt.rgba(1.0, 1.0, 1.0, 1.0) }
                }
            }

            Rectangle
            {
                width: timeline.width
                height: timeline.chunkBarHeight
                anchors.bottom: parent.bottom
                color: "#ffffffff"
            }
        }

        OpacityMask
        {
            id: timelineOpactiyMask

            anchors.fill: timeline
            source: timeline.timelineView
            maskSource: timelineMask
            opacity: Math.min(d.controlsOpacity, d.timelineOpacity, timeline.visible ? 1 : 0)

            Component.onCompleted: timeline.timelineView.visible = false
        }

        Text
        {
            anchors.horizontalCenter: timeline.horizontalCenter
            text: qsTr("No Archive")
            font.capitalization: Font.AllUppercase
            font.pixelSize: 12
            anchors.bottom: timeline.bottom
            anchors.bottomMargin: (timeline.chunkBarHeight - height) / 2 + 12
            color: ColorTheme.windowText
            visible: d.liveMode && !d.hasArchive && videoNavigation.canViewArchive
            opacity: 0.5 * timelineOpactiyMask.opacity
        }

        Pane
        {
            id: buttonsPanel

            readonly property real minimalWidth: width - (zoomButtonsRow.x + zoomButtonsRow.width)
            readonly property bool showZoomControls: actionButtonsPanel.contentWidth < minimalWidth

            width: parent.width
            height: visible ? 56 : 0
            anchors.top: timeline.bottom
            background: Item {}
            padding: 4
            z: 1

            readonly property bool showButtonsPanel: actionButtonsPanel.buttonsCount > 0
            visible: videoNavigation.canViewArchive || showButtonsPanel

            opacity: d.controlsOpacity

            IconButton
            {
                id: calendarButton

                padding: 0
                visible: videoNavigation.canViewArchive
                anchors.verticalCenter: parent.verticalCenter
                icon.source: lp("/images/calendar.png")
                enabled: d.hasArchive
                onClicked:
                {
                    calendarPanel.chunkProvider = cameraChunkProvider
                    calendarPanel.date = timeline.positionDate
                    calendarPanel.open()
                }
            }

            IconButton
            {
                id: motionSearchModeButton

                checked: false
                checkable: true
                anchors.left:  calendarButton.right
                anchors.verticalCenter: parent.verticalCenter
                icon.source: lp("/images/motion.svg")
                icon.width: 24
                icon.height: 24
                padding: 0
                checkedPadding: 4
                normalIconColor: ColorTheme.contrast1
                checkedIconColor: ColorTheme.base1
                visible: videoNavigation.hasArchive
                onCheckedChanged:
                {
                    videoScreenController.mediaPlayer.autoJumpPolicy = checked
                        ? MediaPlayer.DisableAutoJump
                        : MediaPlayer.DisableAutoJumpOnPreviewing
                }
            }

            Row
            {
                id: zoomButtonsRow

                anchors.centerIn: parent
                visible: (buttonsPanel.showZoomControls || !d.liveMode)
                    && videoNavigation.canViewArchive

                IconButton
                {
                    id: zoomOutButton

                    padding: 0
                    icon.source: lp("/images/minus.png")
                    enabled: d.hasArchive
                    onClicked: timeline.zoomOut()
                }

                IconButton
                {
                    id: zoomInButton

                    padding: 0
                    icon.source: lp("/images/plus.png")
                    enabled: d.hasArchive
                    onClicked: timeline.zoomIn()
                }
            }

            Button
            {
                id: liveModeButton

                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("LIVE")
                labelPadding: 0
                rightPadding: 0
                leftPadding: 0
                flat: true
                onClicked:
                {
                    playbackController.checked = false
                    videoScreenController.playLive()
                }

                visible: opacity > 0
                opacity: 0

                property bool shouldBeVisible:
                {
                    var currentTime = (new Date()).getTime()
                    var futurePosition = position > currentTime
                    var canViewArchive = videoNavigation.canViewArchive
                    return canViewArchive && !d.liveMode && !futurePosition
                        && !videoScreenController.resourceHelper.isWearableCamera
                }

                readonly property real position: videoScreenController.mediaPlayer.position
                onPositionChanged: updateOpacity()
                onShouldBeVisibleChanged: updateOpacity()

                function updateOpacity()
                {
                    opacity = shouldBeVisible ? 1 : 0
                }

                Behavior on opacity { NumberAnimation { duration: 200 } }
            }

            ActionButtonsPanel
            {
                id: actionButtonsPanel

                visible: opacity > 0

                resourceId: videoScreenController.resourceId
                anchors.left:
                {
                    if (buttonsPanel.showZoomControls)
                        return zoomButtonsRow.right

                    return motionSearchModeButton.visible
                        ? motionSearchModeButton.right
                        : calendarButton.right
                }

                anchors.right: parent.right
                anchors.rightMargin: -4

                anchors.verticalCenter: parent.verticalCenter

                Binding
                {
                    target: actionButtonsPanel
                    property: "opacity"
                    value:
                    {
                        var futurePosition =
                            videoScreenController.mediaPlayer.position > (new Date()).getTime()
                        var live = d.liveMode || futurePosition
                        return live && buttonsPanel.showButtonsPanel ? 1 : 0
                    }
                }

                onPtzButtonClicked: videoNavigation.ptzButtonClicked()
                onTwoWayAudioButtonPressed: twoWayAudioController.start()
                onTwoWayAudioButtonReleased: twoWayAudioController.stop()

                Behavior on opacity
                {
                    id: actionButtonsPanelOpacityBehaviour

                    NumberAnimation { duration: 200 }
                }

                Binding
                {
                    target: twoWayAudioController
                    property: "resourceId"
                    value: videoScreenController.resourceId
                }
            }

            Timer
            {
                interval: 100
                repeat: true
                running: zoomInButton.down || zoomOutButton.down
                triggeredOnStart: true
                onTriggered:
                {
                    if (zoomInButton.down)
                        timeline.zoomIn()
                    else
                        timeline.zoomOut()
                }
            }
        }

        Item
        {
            id: dateTimeLabel

            visible: videoNavigation.canViewArchive
            height: 48
            width: parent.width
            anchors.bottom: timeline.bottom
            anchors.bottomMargin: timeline.chunkBarHeight + 8
            opacity: d.controlsOpacity

            Text
            {
                id: dateLabel

                anchors.horizontalCenter: parent.horizontalCenter

                height: 20
                font.pixelSize: 13
                font.weight: Font.Normal
                verticalAlignment: Text.AlignVCenter

                text: timeline.positionDate.toLocaleDateString(d.locale, "d MMMM yyyy")
                color: ColorTheme.windowText

                opacity: d.liveMode ? 0.0 : 1.0
                Behavior on opacity { NumberAnimation { duration: 200 } }
            }

            Item
            {
                id: timeLiveLabel

                anchors.horizontalCenter: parent.horizontalCenter

                y: d.liveMode ? (parent.height - height) / 2 : parent.height - height
                Behavior on y { NumberAnimation { duration: 200 } }

                width: timeLabel.visible ? timeLabel.width : liveLabel.width
                height: timeLabel.height

                TimeLabel
                {
                    id: timeLabel
                    dateTime: timeline.positionDate
                    visible: !d.liveMode
                }

                Text
                {
                    id: liveLabel
                    anchors.verticalCenter: parent.verticalCenter
                    font.pixelSize: 28
                    font.weight: Font.Normal
                    color: ColorTheme.windowText
                    text: qsTr("LIVE")
                    visible: d.liveMode
                }
            }
        }

        Text
        {
            id: liveOnlyText

            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            verticalAlignment: Text.AlignVCenter
            height: 56
            font.pixelSize: 28
            font.weight: Font.Normal
            color: ColorTheme.windowText
            text: qsTr("LIVE")
            visible: d.liveMode && !videoNavigation.canViewArchive
        }

        PlaybackController
        {
            id: playbackController

            visible: videoNavigation.canViewArchive

            anchors.top: navigator.top
            anchors.horizontalCenter: parent.horizontalCenter

            loading: videoScreenController.mediaPlayer.loading || timeline.dragging
            paused: videoNavigation.paused

            opacity: d.controlsOpacity

            ChunkPositionWatcher
            {
                id: chunkPositionWatcher
                motionSearchMode: videoNavigation.motionSearchMode
                position: timeline.position
                chunkProvider: timeline.chunkProvider
            }

            PlaybackJumpButton
            {
                forward: false
                visible: d.hasArchive && videoNavigation.canViewArchive
                anchors.right: parent.left
                anchors.verticalCenter: parent.verticalCenter

                onClicked: d.goToPosition(chunkPositionWatcher.prevChunkStartTimeMs(), false)
            }

            PlaybackJumpButton
            {
                enabled: !d.liveMode
                visible: d.hasArchive && videoNavigation.canViewArchive
                anchors.left: parent.right
                anchors.verticalCenter: parent.verticalCenter

                onClicked:
                {
                    var nextChunkStartTime = chunkPositionWatcher.nextChunkStartTimeMs();
                    d.goToPosition(nextChunkStartTime, false)
                    if (nextChunkStartTime == -1)
                        videoScreenController.play()
                }
            }

            onClicked:
            {
                if (paused)
                {
                    if (d.resumePosition > 0)
                    {
                        videoScreenController.setPosition(d.resumePosition)
                        d.resumePosition = -1
                    }
                    videoScreenController.play()
                }
                else
                {
                    if (d.liveMode)
                        d.resumePosition = videoScreenController.mediaPlayer.position
                    videoScreenController.pause()
                }
            }
        }

        Rectangle
        {
            color: ColorTheme.windowText
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            width: 2
            height: 20
            visible: d.hasArchive && videoNavigation.canViewArchive
            opacity: timelineOpactiyMask.opacity
        }
    }

    Rectangle
    {
        // This rectangle guarantees the same color under android navigation buttons as under the navigation panel
        width: parent.width
        anchors.top: parent.bottom
        color: "black"
        height: buttonsPanel.height
    }

    CalendarPanel
    {
        id: calendarPanel

        onDatePicked:
        {
            close()
            d.resumePosition = -1
            // TODO: Make refactoring and get rid of serverTimeZoneShift (etc) properties.
            // Timeline and calendar should work in a same way.
            var targetTime = date.getTime() - timeline.serverTimeZoneShift
            timeline.jumpTo(targetTime)
            videoScreenController.setPosition(targetTime, true)
        }
    }

    Connections
    {
        target: videoScreenController
        onPlayerJump:
        {
            timeline.autoReturnToBounds = false
            timeline.jumpTo(position)
        }
        onGotFirstPosition:
        {
            timeline.autoReturnToBounds = false
            timeline.jumpTo(position)
        }
    }

    Component.onCompleted:
    {
        d.updateNavigatorPosition()
        liveModeButton.opacity = 0
    }
}
