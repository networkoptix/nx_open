import QtQuick 2.4;

import ".";
import "standard" as Rtu;

Grid
{
    id: thisComponent;

    property bool changed: (useDHCPCheckBox.changed || ipControl.changed || subnetMask.changed);
    property bool isSingleSelectionModel: false;

    property var changesHandler;

    readonly property alias useDHCPControl: useDHCPCheckBox;
    readonly property alias ipAddressControl: ipControl;
    readonly property alias subnetMaskControl: subnetMask;

    property string adapterNameValue;

    spacing: SizeManager.spacing.small;

    columns: 3;


    Rtu.Text
    {
        id: adapterName;

        height: SizeManager.clickableSizes.base;

        text: (thisComponent.isSingleSelectionModel ? readableName: qsTr("Multiple interfaces"));
        verticalAlignment: Text.AlignVCenter;
    }

    Rtu.Text
    {
        id: subnetMaskCaption;

        height: SizeManager.clickableSizes.base;

        text: (thisComponent.isSingleSelectionModel && (model.index === 0) ? "Mask": " ");
        verticalAlignment: Text.AlignVCenter;
    }

    Item
    {
        id: fakeEmptyItem;
        width: 1;
        height: 1;
    }

    Rtu.TextField
    {
        id: ipControl

        changesHandler: thisComponent.changesHandler;

        enabled: !useDHCPCheckBox.checked && thisComponent.isSingleSelectionModel;
        opacity: ( enabled ? 1 : 0.5 );
    }

    Rtu.TextField
    {
        id: subnetMask;

        changesHandler: thisComponent.changesHandler;

        enabled: !useDHCPCheckBox.checked && thisComponent.isSingleSelectionModel;
        opacity: ( enabled ? 1 : 0.5 );
    }

    Rtu.CheckBox
    {
        id: useDHCPCheckBox;

        height: subnetMask.height;
        text: qsTr("Use DHCP");

        changesHandler: thisComponent.changesHandler;
    }
}
