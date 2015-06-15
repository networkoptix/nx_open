import QtQuick 2.1;

import "../base" as Base;
import "../../common" as Common;

Grid
{
    id: thisComponent;

    property bool changed: (useDHCPCheckBox.changed || ipControl.changed || subnetMask.changed);
    property bool isSingleSelectionModel: false;

    readonly property alias useDHCPControl: useDHCPCheckBox;
    readonly property alias ipAddressControl: ipControl;
    readonly property alias subnetMaskControl: subnetMask;

    property string adapterNameValue;
    
    
    Component.onCompleted: console.log("s" + spacing);

    spacing: Common.SizeManager.spacing.base;
    columns: 3;


    Base.Text
    {
        id: adapterName;

        text: (thisComponent.isSingleSelectionModel ? readableName: qsTr("Multiple interfaces"));
    }

    Base.Text
    {
        id: subnetMaskCaption;

        text: (thisComponent.isSingleSelectionModel && (model.index === 0) ? "Mask": " ");
    }

    Item
    {
        id: fakeEmptyItem;
        width: 1;
        height: 1;
    }

    Base.IpControl
    {
        id: ipControl

        enabled: !useDHCPCheckBox.checked && thisComponent.isSingleSelectionModel;
        opacity: ( enabled ? 1 : 0.5 );
    }

    Base.IpControl
    {
        id: subnetMask;

        enabled: !useDHCPCheckBox.checked && thisComponent.isSingleSelectionModel;
        opacity: ( enabled ? 1 : 0.5 );
    }

    Base.CheckBox
    {
        id: useDHCPCheckBox;

        height: subnetMask.height;
        text: qsTr("Use DHCP");
    }
}
