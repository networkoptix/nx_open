import QtQuick 2.5;
import QtQuick.Controls 1.2;
import Nx.Models 1.0;

MaskedItem
{
    id: control;

    property var model;
    property string comboBoxTextRole: "display";
    property string comboBoxValueRole: comboBoxTextRole;
    property bool isEditableComboBox: false;
    property int currentItemIndex: (maskedArea ? maskedArea.currentIndex : -1);

    function forceCurrentIndex(index)
    {
        if (maskedArea)
            maskedArea.currentIndex = index;
    }

    maskedAreaDelegate: NxComboBox
    {
        id: comboBox;

        textRole: control.comboBoxTextRole;
        model: control.model;
        width: control.width;
        editable: control.isEditableComboBox;
        isEditMode: editable;

        onTextChanged:
        {
            control.displayValue = text;

            if (control.currentItemIndex == -1)
            {
                control.value = text;
                return;
            }

            control.value = dataAccessor.getData(
                control.currentItemIndex, control.comboBoxValueRole);
        }

        KeyNavigation.tab: control.KeyNavigation.tab;
        KeyNavigation.backtab: control.KeyNavigation.backtab;
        onAccepted: control.accepted();

        ModelDataAccessor
        {
            id: dataAccessor;
            model: control.model;
        }
    }
}
