import Nx.Controls 1.0

import "private"

LabeledItem
{
    id: control

    property int defaultValue: 0
    property alias minValue: spinBox.from
    property alias maxValue: spinBox.to
    property alias value: spinBox.value

    contentItem: SpinBox
    {
        id: spinBox
        editable: true
        value: defaultValue
    }
}
