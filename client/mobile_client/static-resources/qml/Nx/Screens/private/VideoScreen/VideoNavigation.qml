import QtQuick 2.6
import QtGraphicalEffects 1.0
import QtQuick.Window 2.2
import Qt.labs.controls 1.0
import Nx 1.0
import Nx.Media 1.0
import Nx.Controls 1.0
import com.networkoptix.qml 1.0

Item
{
    id: videoNavigation

    property string resourceId
    property var videoScreenController
    property bool paused: videoScreenController.mediaPlayer.playbackState !== MediaPlayer.Playing

    property real controlsOpacity: 1.0

    implicitWidth: parent ? parent.width : 0
    implicitHeight: navigator.height + navigationPanel.height
    anchors.bottom: parent ? parent.bottom : undefined

    QtObject
    {
        id: d

        property real controlsOpacity: videoNavigation.controlsOpacity
        property real timelineOpacity: cameraChunkProvider.loading ? 0.0 : 1.0
        Behavior on timelineOpacity
        {
            NumberAnimation { duration: d.timelineOpacity > 0 ? 0 : 200 }
        }

        readonly property bool hasArchive: timeline.startBound > 0
        readonly property bool liveMode:
            videoScreenController
                && videoScreenController.mediaPlayer.liveMode
                && !playbackController.paused
        property real resumePosition: -1

        function updateNavigatorPosition()
        {
            if (Screen.primaryOrientation == Qt.PortraitOrientation)
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
    }

    Timer
    {
        interval: 30000
        triggeredOnStart: true
        running: true
        repeat: true
        onTriggered: cameraChunkProvider.update()
    }

    Item
    {
        id: navigator

        width: parent.width
        height: timeline.height + playbackController.height
        Behavior on y { SmoothedAnimation { duration: 200; reversingMode: SmoothedAnimation.Sync } }

        MouseArea
        {
            id: navigatorMouseArea

            anchors.fill: navigator
            drag.axis: Drag.YAxis
            drag.minimumY: 0
            drag.maximumY: navigationPanel.height
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

        Image
        {
            width: parent.width
            anchors.bottom: timeline.bottom
            height: timeline.chunkBarHeight
            source: lp("/images/timeline_chunkbar_preloader.png")
            sourceSize: Qt.size(timeline.chunkBarHeight, timeline.chunkBarHeight)
            fillMode: Image.Tile
        }

        Image
        {
            width: timeline.width
            height: sourceSize.height
            anchors.bottom: timeline.bottom
            anchors.bottomMargin: timeline.chunkBarHeight
            sourceSize.height: 150 - timeline.chunkBarHeight
            source: lp("/images/timeline_gradient.png")
        }

        Timeline
        {
            id: timeline

            property bool resumeWhenDragFinished: false

            enabled: d.hasArchive

            anchors.bottom: parent.bottom
            width: parent.width
            height: 104

            stickToEnd: d.liveMode && !paused

            chunkBarHeight: 32
            textY: height - chunkBarHeight - 16 - 24

            chunkProvider: cameraChunkProvider
            startBound: cameraChunkProvider.bottomBound

            onMovingChanged:
            {
                if (!moving)
                {
                    videoScreenController.setPosition(position)
                    if (resumeWhenDragFinished)
                        videoScreenController.play()
                    else
                        videoScreenController.pause()
                }
            }
            onPositionTapped:
            {
                d.resumePosition = -1
                videoScreenController.setPosition(position)
            }
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

            Binding
            {
                target: timeline
                property: "position"
                value: videoScreenController.mediaPlayer.position
                when: !timeline.moving && !d.liveMode
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
            opacity: Math.min(d.controlsOpacity, d.timelineOpacity)

            Component.onCompleted: timeline.timelineView.visible = false
        }

        Text
        {
            anchors.horizontalCenter: timeline.horizontalCenter
            text: qsTr("No Archive")
            font.capitalization: Font.AllUppercase
            font.pixelSize: 12
            anchors.bottom: timeline.bottom
            anchors.bottomMargin: (timeline.chunkBarHeight - height) / 2
            color: ColorTheme.windowText
            visible: !d.hasArchive
            opacity: 0.5 * timelineOpactiyMask.opacity
        }

        Pane
        {
            id: navigationPanel

            width: parent.width
            height: 64
            anchors.top: timeline.bottom
            background: Rectangle { color: ColorTheme.base3 }
            padding: 4

            IconButton
            {
                anchors.verticalCenter: parent.verticalCenter
                icon: lp("/images/calendar.png")
                enabled: d.hasArchive
                onClicked:
                {
                    calendarPanelLoader.active = true
                    calendarPanelLoader.item.date = timeline.positionDate
                    calendarPanelLoader.item.open()
                }
            }

            Row
            {
                anchors.centerIn: parent

                IconButton
                {
                    id: zoomOutButton

                    icon: lp("/images/minus.png")
                    enabled: d.hasArchive
                    onClicked: timeline.zoomOut()
                }

                IconButton
                {
                    id: zoomInButton

                    icon: lp("/images/plus.png")
                    enabled: d.hasArchive
                    onClicked: timeline.zoomIn()
                }
            }

            Button
            {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("LIVE")
                flat: true
                onClicked:
                {
                    playbackController.checked = false
                    videoScreenController.playLive()
                }
                opacity: d.liveMode ? 0.0 : 1.0
                Behavior on opacity { NumberAnimation { duration: 200 } }
            }

            Timer
            {
                interval: 100
                repeat: true
                running: zoomInButton.pressed || zoomOutButton.pressed
                triggeredOnStart: true
                onTriggered:
                {
                    if (zoomInButton.pressed)
                        timeline.zoomIn()
                    else
                        timeline.zoomOut()
                }
            }
        }

        Item
        {
            id: dateTimeLabel

            height: 56
            width: parent.width
            anchors.bottom: timeline.bottom
            anchors.bottomMargin: timeline.chunkBarHeight + 16
            opacity: d.controlsOpacity

            Text
            {
                id: dateLabel

                anchors.horizontalCenter: parent.horizontalCenter

                height: 24
                font.pixelSize: 14
                font.weight: Font.Normal
                verticalAlignment: Text.AlignVCenter

                // TODO: Remove qsTr from this string!
                text: timeline.positionDate.toLocaleDateString(d.locale, qsTr("d MMMM yyyy", "DO NOT TRANSLATE THIS STRING!"))
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
                    font.pixelSize: 32
                    font.weight: Font.Normal
                    color: ColorTheme.windowText
                    text: qsTr("LIVE")
                    visible: d.liveMode
                }
            }
        }

        PlaybackController
        {
            id: playbackController

            anchors.verticalCenter: timeline.bottom
            anchors.verticalCenterOffset: -150
            anchors.horizontalCenter: parent.horizontalCenter

            loading: !paused && (videoScreenController.mediaPlayer.loading || timeline.dragging)
            paused: videoNavigation.paused

            opacity: d.controlsOpacity

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
            anchors.bottom: playbackController.bottom
            width: 2
            height: 8
            visible: d.hasArchive
        }

        Rectangle
        {
            color: ColorTheme.windowText
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            width: 2
            height: timeline.chunkBarHeight + 8
            visible: d.hasArchive
        }
    }

    Rectangle
    {
        // This rectangle guarantees the same color under android navigation buttons as under the navigation panel
        width: parent.width
        anchors.top: parent.bottom
        color: ColorTheme.base3
        height: navigationPanel.height
    }

    Component
    {
        id: calendarPanelComponent

        CalendarPanel
        {
            chunkProvider: cameraChunkProvider
            onDatePicked:
            {
                close()
                d.resumePosition = -1
                videoScreenController.setPosition(date.getTime())
            }
        }
    }

    Loader
    {
        id: calendarPanelLoader
        sourceComponent: calendarPanelComponent
        active: false
        y: parent.height - height
    }

    Component.onCompleted: d.updateNavigatorPosition()

    Connections
    {
        target: videoScreenController
        onPlayerJump: timeline.jumpTo(position)
    }
}
