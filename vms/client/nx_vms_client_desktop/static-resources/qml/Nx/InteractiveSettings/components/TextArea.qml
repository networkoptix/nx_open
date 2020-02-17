import Nx.Controls 1.0

import "private"

LabeledItem
{
    id: control

    property alias value: textField.text
    property string defaultValue: ""

    readonly property bool filled: textField.text !== ""

    contentItem: TextArea
    {
        id: textField
        text: defaultValue
    }
}

