import QtQuick 2.6

Rectangle
{
    id: control

    color: "darkblue" // todo remove me
    readonly property int kEightWayPtz: 0
    readonly property int kFourWayPtz: 1

    /**
     * "direction" property specifies blah blah TODO: write comment
     */
    property point direction: _private.positionToDirection(markerPosition)
    property point markerPosition: supportDrag && mouseArea.pressed
        ? Qt.point(mouseArea.mouseX, mouseArea.mouseY)
        : _private.centerPoint

    property bool supportDrag: false
    property bool supportSingleShot: false

    signal singleShot(point direction)
    signal buttonPressed(point direction)
    signal buttonReleased()

    implicitWidth: 136;
    implicitHeight: implicitWidth
    radius: width / 2

    Grid
    {
        padding: 4;
        width: parent.width - padding * 2;
        height: parent.height - padding * 2;

        columns: 3
        rows: 3
        spacing: 16

        Repeater
        {
            model: 9

            Component
            {
                id: buttonDelegate

                Text
                {
                    text: _private.kTempTexts[Repeater.index]
                }
            }

            Component
            {
                id: emptyDelegate

                Item {}
            }

            delegate: _private.kButtonsMask[Repeater.index] ? buttonDelegate : emptyDelegate
        }
    }

    Rectangle
    {
        id: draggableMarker

        property point currentPosition: _private.directionToPosition(direction);

        x: currentPosition.x - width / 2
        y: currentPosition.y - height / 2

        width: 32
        height: width
        radius: width / 2

        visible: control.supportDrag
        color: "transparent"
        border.color: "white" // todo replace with actual color
    }

    MouseArea
    {
        id: mouseArea

        anchors.fill: parent
        hoverEnabled: true
    }

    QtObject
    {
        id: _private;

        readonly property var kStaticDirections: [
            Qt.point(-1, 1), Qt.point(0, 1), Qt.point(1, 1),
            Qt.point(-1, 0), Qt.point(0, 0), Qt.point(1, 0),
            Qt.point(-1, -1), Qt.point(0, -1), Qt.point(1, -1)]

        readonly property var kButtonsMask: [
            true, true, false,
            true, true, true,
            false, true, true]

        readonly property var kTempTexts: [
            "1", "^", "",
            "<", "o", ">",
            "", "v", ""];

        property real maxLength: (control.width - draggableMarker.width) / 2
        property point centerPoint: Qt.point(control.width / 2, control.width / 2)

        function positionToDirection(position)
        {
            var vector = Qt.point(position.x - centerPoint.x, position.y - centerPoint.y);
            var length = Math.sqrt(Math.pow(vector.x, 2) + Math.pow(vector.y, 2))
            var aspect = 1 / Math.max(length, maxLength)
            return Qt.point(vector.x * aspect, vector.y * aspect)
        }

        function directionToPosition(direction)
        {
            var vector = Qt.point(direction.x * maxLength, direction.y * maxLength)
            return Qt.point(centerPoint.x + vector.x, centerPoint.y + vector.y)
        }
    }
}

