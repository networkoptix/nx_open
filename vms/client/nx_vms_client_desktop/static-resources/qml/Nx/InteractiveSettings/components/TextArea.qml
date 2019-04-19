import Nx.Controls 1.0

import "private"

LabeledItem
{
    id: control

    property alias value: textField.text
    property string defaultValue: ""

    contentItem: TextArea
    {
        id: textField
        text: defaultValue
    }
}

