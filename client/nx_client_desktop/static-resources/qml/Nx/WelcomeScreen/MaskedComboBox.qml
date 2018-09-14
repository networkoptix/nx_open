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
            control.value =
                dataAccessor.getData(currentIndex, control.comboBoxValueRole) || displayText
        }

        onCountChanged:
        {
            if (count > 0 && currentIndex === -1)
                currentIndex = 0
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
