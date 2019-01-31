import QtQuick 2.0
import Nx.Controls 1.0

import "private"

LabeledItem
{
    id: control

    property var value: defaultValue
    property var defaultValue
    property alias range: comboBox.model

    onValueChanged: comboBox.currentIndex = range.indexOf(value)

    contentItem: ComboBox
    {
        id: comboBox
        onActivated: control.value = currentText
    }
}
