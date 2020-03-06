import QtQuick 2.6
import QtQuick.Layouts 1.11

Item
{
    id: control

    property var items: []

    property color nameColor: "gray"
    property color valueColor: "white"
    property font font

    height: grid.implicitHeight

    GridLayout
    {
        id: grid

        columns: 2
        width: control.width
        columnSpacing: 8
        rowSpacing: 0

        Repeater
        {
            id: repeater

            model: items

            Text
            {
                id: itemText

                readonly property bool isLabel: (index % 2) === 0

                color: isLabel ? control.nameColor : control.valueColor
                font: control.font
                text: modelData
                textFormat: Text.StyledText
                maximumLineCount: 2
                wrapMode: Text.Wrap
                elide: Text.ElideRight

                Layout.alignment: Qt.AlignLeft | Qt.AlignBaseline
                Layout.fillWidth: !isLabel
                Layout.maximumWidth: isLabel
                    ? (grid.width - grid.columnSpacing) / 2
                    : -1
            }
        }
    }
}
