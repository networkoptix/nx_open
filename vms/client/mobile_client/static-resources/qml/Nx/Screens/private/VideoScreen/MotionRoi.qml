import QtQuick 2.6
import QtGraphicalEffects 1.0
import Nx.Core.Items 1.0

Item
{
    id: item

    property point startPoint
    property point endPoint
    property bool drawing: false

    property color roiColor
    property color shadowColor
    property int lineWidth: 1
    property bool singlePoint: true

    readonly property alias expanded: singleSelectionMarker.expanded

    x: d.topLeftPoint.x
    y: d.topLeftPoint.y

    width: d.bottomRightPoint.x - d.topLeftPoint.x + 1
    height: d.bottomRightPoint.y - d.topLeftPoint.y + 1

    // Shows preloader
    function start()
    {
        startDelayTimer.restart()
    }

    // Shows selection
    function show()
    {
        startDelayTimer.stop()
        d.visible = true
        d.preloader = false
    }

    // Hides whatever it shows.
    function hide()
    {
        startDelayTimer.stop()
        d.preloader = false
        d.visible = false
    }

    Timer
    {
        id: startDelayTimer

        interval: 200
        repeat: false
        onTriggered: d.preloader = true
    }

    RoiSelectionPreloader
    {
        id: singleSelectionMarker

        mainColor: item.roiColor
        shadowColor: item.shadowColor
        state:
        {
            if (d.preloader || item.drawing)
                return "expanded"

            return d.visible && item.singlePoint ? "normal" : "hidden"
        }

        centerPoint: d.endMarkerPoint
        visible: opacity > 0
    }

    Rectangle
    {
        id: roiMarker

        visible: (item.drawing || !item.singlePoint) && d.visible
        anchors.fill: parent
        border.width: d.shadowRadius * 2 + boundingLines.border.width
        border.color: item.shadowColor
        color: "transparent"

        Rectangle
        {
            id: boundingLines

            x: d.shadowRadius
            y: d.shadowRadius
            width: parent.width - d.shadowRadius * 2
            height: parent.height - d.shadowRadius * 2
            color: "transparent"

            border.width: item.lineWidth
            border.color: item.roiColor

        }

        ShadedCircle
        {
            visible: item.drawing
            circleRadius: 2.5
            centerPoint: d.startMarkerPoint
            shadowColor: item.shadowColor
            circleColor: item.roiColor
        }

        ShadedCircle
        {
            visible: item.drawing
            circleRadius: 2.5
            centerPoint: d.endMarkerPoint
            shadowColor: item.shadowColor
            circleColor: item.roiColor
        }
    }

    QtObject
    {
        id: d

        property bool visible: false
        property bool preloader: false
        readonly property int shadowRadius: 1
        readonly property real subpixelOffset: 0.5
        readonly property real markerOffset: shadowRadius + subpixelOffset
        readonly property real markerRight: item.width - shadowRadius - subpixelOffset
        readonly property real markerBottom: item.height - shadowRadius - subpixelOffset

        property point topLeftPoint: Qt.point(
            Math.min(item.startPoint.x, item.endPoint.x) - shadowRadius,
            Math.min(item.startPoint.y, item.endPoint.y) - shadowRadius)
        property point bottomRightPoint: Qt.point(
            Math.max(item.startPoint.x, item.endPoint.x) + shadowRadius,
            Math.max(item.startPoint.y, item.endPoint.y) + shadowRadius)

        property point startMarkerPoint: Qt.point(
            item.startPoint.x < item.endPoint.x ? markerOffset : markerRight,
            item.startPoint.y < item.endPoint.y ? markerOffset : markerBottom)

        property point endMarkerPoint: Qt.point(
            item.startPoint.x > item.endPoint.x ? markerOffset : markerRight,
            item.startPoint.y > item.endPoint.y ? markerOffset : markerBottom)
    }
}

