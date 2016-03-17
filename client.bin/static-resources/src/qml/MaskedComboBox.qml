import QtQuick 2.5;
import QtQuick.Controls 1.2;

MaskedItem
{
    id: thisComponent;

    property var model;
    property string comboBoxTextRole: "display";

    maskedAreaDelegate: ComboBox
    {
        id: comboBox;

        textRole: thisComponent.comboBoxTextRole;
        model: thisComponent.model;
        width: thisComponent.width;

        onCurrentTextChanged: { thisComponent.value = currentText; }
    }
}
