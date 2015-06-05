import QtQuick 2.4;
import QtQuick.Controls 1.3;

import "../controls";
import "standard" as Rtu;

Column
{
    id: thisComponent;
    
    property var changesHandler;


    spacing: SizeManager.spacing.base;
    
    function applyButtonPressed()
    {
        if (!thisComponent.changesHandler || !thisComponent.changesHandler.changed)
            return;

        var children = column.children;
        for (var i = 0; i !== children.length; ++i)
        {
            var item = children[i];
            if (!item.hasOwnProperty("macAddressValue")|| !item.hasOwnProperty("isSingleSelectionModel") 
                || !item.hasOwnProperty("changed") || !item.changed)
            {
                continue;
            }
            
            var useDHCP = (item.useDHCPControl.checkedState !== Qt.Unchecked ? true : false);
            var ipAddress = (item.ipAddressControl.changed && useDHCP ? item.ipAddressControl.text : "");
            var subnetMask = (item.subnetMaskControl.changed && useDHCP ? item.subnetMaskControl.text : "");
            rtuContext.changesManager().addIpChangeRequest(
                item.macAddressValue, useDHCP, ipAddress, subnetMask);
        }
    }
    
    Column
    {
        id: column;
        spacing: SizeManager.spacing.base;

        anchors
        {
            left: parent.left;            
            leftMargin: SizeManager.spacing.base;
        }

        Repeater
        {
            id: repeater;
            model: rtuContext.ipSettingsModel();
            
            delegate: IpChangeLine
            {
                changesHandler: thisComponent.changesHandler;        
                
                isSingleSelectionModel: (repeater.model ? repeater.model.isSingleSelection : true);
                
                useDHCPControl.initialCheckedState: useDHCP;
                ipAddressControl.initialText: (isSingleSelectionModel ? address : qsTr("Multiple interfaces"));
                subnetMaskControl.initialText: (isSingleSelectionModel ? subnetMask : qsTr("Multiple interfaces"));
                
                adapterNameValue: adapterName;
                macAddressValue: macAddress;
            }
        }
    }

    Rtu.Text
    {
        wrapMode: Text.Wrap;

        anchors.left: column.left;
        
        color: "darkred";
        text: qsTr("Ip address and subnet mask cannot be changed for multiple servers");
        
        visible: !repeater.model.isSingleSelection;
    }
}

