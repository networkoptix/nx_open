import QtQuick 2.6
import QtGraphicalEffects 1.0
import Nx.Core.Items 1.0

// Test control for now. Represents simple ROI

Item
{
    id: item

    property point startPoint
    property point endPoint
    property bool drawing: false

    property color roiColor
    property color shadowColor

    readonly property bool singlePoint:
        Qt.vector2d(startPoint.x - endPoint.x, startPoint.y - endPoint.y).length() < 5

    x: d.topLeftPoint.x
    y: d.topLeftPoint.y

    width: d.bottomRightPoint.x - d.topLeftPoint.x + 1
    height: d.bottomRightPoint.y - d.topLeftPoint.y + 1

    ShadedCircle
    {
        id: singlePointMarker

        centerPoint: d.startMarkerPoint
        circleRadius: 4.5
        visible: singlePoint
        circleColor: "transparent"
        circleBorderColor: item.roiColor
        shadowColor: item.roiColor

        border.color: roiColor
    }

    Rectangle
    {
        id: roiMarker

        visible: !singlePointMarker.visible

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

            border.width: 1
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

        readonly property real subpixelOffset: 0.5
        readonly property real markerOffset: shadowRadius + subpixelOffset
        readonly property real markerRight: item.width - shadowRadius - subpixelOffset
        readonly property real markerBottom: item.height - shadowRadius - subpixelOffset
        property int shadowRadius: /*signlePoint ? 0 :*/ 20
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

