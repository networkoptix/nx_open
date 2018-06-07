import QtQuick 2.5
import Nx.Models 1.0

MaskedItem
{
    id: control

    property var model
    property string comboBoxTextRole: "display"
    property string comboBoxValueRole: comboBoxTextRole
    property int currentItemIndex: (maskedArea ? maskedArea.currentIndex : -1)

    function forceCurrentIndex(index)
    {
        if (maskedArea)
            maskedArea.currentIndex = index
    }

    maskedAreaDelegate: NxComboBox
    {
        textRole: control.comboBoxTextRole
        model: control.model
        width: control.width
        editable: true

        onDisplayTextChanged:
        {
            control.displayValue = displayText

            // TODO: Fix getData call which unexpectedly returns undefined.
            control.value = (currentIndex === -1)
                ? displayText
                : dataAccessor.getData(currentIndex, control.comboBoxValueRole)
        }

        KeyNavigation.tab: control.KeyNavigation.tab
        KeyNavigation.backtab: control.KeyNavigation.backtab
        onAccepted: control.accepted()
    }

    ModelDataAccessor
    {
        id: dataAccessor
        model: control.model
    }
}
