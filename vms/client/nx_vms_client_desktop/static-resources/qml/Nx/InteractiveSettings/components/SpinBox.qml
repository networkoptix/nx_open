import QtQuick 2.6

import Nx.Controls 1.0

import "private"

LabeledItem
{
    id: control

    property int defaultValue: 0
    property alias minValue: spinBox.from
    property alias maxValue: spinBox.to
    property alias value: spinBox.value

    contentItem: Item
    {
        implicitWidth: spinBox.implicitWidth
        implicitHeight: spinBox.implicitHeight
        baselineOffset: spinBox.baselineOffset

        SpinBox
        {
            id: spinBox

            editable: true
            value: defaultValue
        }
    }
}
