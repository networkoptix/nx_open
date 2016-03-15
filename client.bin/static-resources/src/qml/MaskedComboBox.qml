import QtQuick 2.5;
import QtQuick.Controls 1.2;

MaskedItem
{
    id: thisComponent;

//    property string value: area.currentText;
    height: area.height;

    maskedAreaDelegate: ComboBox
    {
        id: comboBox;

        width: thisComponent.width;
    }
}
