import QtQuick 2.5;
import QtQuick.Controls 1.2;

MaskedItem
{
    id: thisComponent;

    property var model;
    property string comboBoxTextRole: "display";
    property bool isEditableComboBox: false;

    maskedAreaDelegate: NxComboBox
    {
        id: comboBox;

        textRole: thisComponent.comboBoxTextRole;
        model: thisComponent.model;
        width: thisComponent.width;
        editable: thisComponent.isEditableComboBox;
        isEditMode: editable;

        onTextChanged: { thisComponent.value = text; }
    }
}
