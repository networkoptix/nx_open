import QtQuick 2.0
import Nx.Controls 1.0

import "private"

LabeledItem
{
    id: control

    property alias value: checkBox.checked
    property bool defaultValue: false

    contentItem: checkBox

    CheckBox
    {
        id: checkBox
        checked: defaultValue
    }

    function setValue(value)
    {
        control.value = (value === true || value === "true" || value === 1)
    }
}
