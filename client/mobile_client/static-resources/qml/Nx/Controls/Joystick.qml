import QtQuick 2.6

Grid
{
    id: control

    /**
     * "direction" property specifies blah blah TODO: write comment
     */
    property point direction;
    property bool supportDrag: false;
    property bool supportSingleShot: false;

    signal singleShot(point direction)

    columns: 3
    rows: 3
    spacing: 0

    Repeater
    {
        model: 9

        Button
        {
            width: 50
            height: 50

            text: _private.kTempTexts[index]
            onClicked: { singleShot(_private.kStaticDirections[index]) }
        }
    }

    QtObject
    {
        id: _private;

        readonly property var kStaticDirections: [
            Qt.point(-1, 1), Qt.point(0, 1), Qt.point(1, 1),
            Qt.point(-1, 0), Qt.point(0, 0), Qt.point(1, 0),
            Qt.point(-1, -1), Qt.point(0, -1), Qt.point(1, -1)];

        readonly property var kTempTexts: [
            "", "^", "",
            "<", "o", ">",
            "", "v", ""];
    }
}
