import QtQuick 2.5;
import QtQuick.Controls 1.2;

MaskedItem
{
    id: thisComponent;

    property var model;
    property string comboBoxTextRole: "display";
    property bool isEditableComboBox: false;
    property int currentItemIndex: (maskedArea ? maskedArea.currentIndex : -1);

    maskedAreaDelegate: NxComboBox
    {
        id: comboBox;

        textRole: thisComponent.comboBoxTextRole;
        model: thisComponent.model;
        width: thisComponent.width;
        editable: thisComponent.isEditableComboBox;
        isEditMode: editable;

        onVisibleChanged:
        {
            if (editable && visible)
                forceActiveFocus();
        }

        onTextChanged: { thisComponent.value = text; }

        KeyNavigation.tab: thisComponent.KeyNavigation.tab;
        KeyNavigation.backtab: thisComponent.KeyNavigation.backtab;
    }
}
