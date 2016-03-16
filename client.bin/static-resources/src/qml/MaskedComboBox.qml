import QtQuick 2.5;
import QtQuick.Controls 1.2;

MaskedItem
{
    id: thisComponent;

    property var model;

    height: area.height;

    maskedAreaDelegate: ComboBox
    {
        id: comboBox;

        textRole: "display";
        model: thisComponent.model;
        width: thisComponent.width;

        onCurrentTextChanged: { thisComponent.value = currentText; }
    }
}
