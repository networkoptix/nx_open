import QtQuick 2.5;
import QtQuick.Controls 1.2;

MaskedItem
{
    id: thisComponent;

    property var model;
    property string comboBoxTextRole: "display";
    property bool isEditableComboBox: false;

    maskedAreaDelegate: ComboBox
    {
        id: comboBox;

        textRole: thisComponent.comboBoxTextRole;
        model: thisComponent.model;
        width: thisComponent.width;
        editable: thisComponent.isEditableComboBox;

        onCurrentTextChanged: { thisComponent.value = currentText; }
    }
}
