import QtQuick 2.0

import Nx.Controls 1.0

import "private"

LabeledItem
{
    id: control

    property real defaultValue: 0
    property alias minValue: spinBox.dFrom
    property alias maxValue: spinBox.dTo
    property alias value: spinBox.dValue
    property alias decimals: spinBox.decimals
    property alias step: spinBox.dStepSize

    contentItem: Item
    {
        implicitWidth: spinBox.implicitWidth
        implicitHeight: spinBox.implicitHeight
        baselineOffset: spinBox.baselineOffset

        DoubleSpinBox
        {
            id: spinBox

            editable: true
            dValue: defaultValue
        }
    }
}
