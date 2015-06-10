import QtQuick 2.1;
import QtQuick.Controls 1.1;

import "." as Rtu;
import "../base" as Base;
import "../../common" as Common;

Base.Column
{
    id: thisComponent;
    
    property var changesHandler;

    function applyButtonPressed()
    {
        if (!thisComponent.changesHandler || !thisComponent.changesHandler.changed)
            return;

        var children = column.children;
        for (var i = 0; i !== children.length; ++i)
        {
            var item = children[i];
            if (!item.hasOwnProperty("adapterNameValue")|| !item.hasOwnProperty("isSingleSelectionModel") 
                || !item.hasOwnProperty("changed") || !item.changed)
            {
                continue;
            }
            
            var useDHCP = (item.useDHCPControl.checkedState !== Qt.Unchecked ? true : false);
            var ipAddress = (item.ipAddressControl.changed && !useDHCP ? item.ipAddressControl.text : "");
            var subnetMask = (item.subnetMaskControl.changed && !useDHCP ? item.subnetMaskControl.text : "");
            rtuContext.changesManager().addIpChangeRequest(
                item.adapterNameValue, useDHCP, ipAddress, subnetMask);
        }
    }
    
    Base.Column
    {
        id: column;

        anchors
        {
            left: parent.left;            
            leftMargin: Common.SizeManager.spacing.base;
        }

        Repeater
        {
            id: repeater;
            model: rtuContext.ipSettingsModel();
            
            delegate: Rtu.IpChangeLine
            {
                changesHandler: thisComponent.changesHandler;        
                
                isSingleSelectionModel: (repeater.model ? repeater.model.isSingleSelection : true);
                
                useDHCPControl.initialCheckedState: useDHCP;
                ipAddressControl.initialText: (isSingleSelectionModel ? address : qsTr("Multiple interfaces"));
                subnetMaskControl.initialText: (isSingleSelectionModel ? subnetMask : qsTr("Multiple interfaces"));
                
                adapterNameValue: adapterName;
            }
        }
    }

    Base.Text
    {
        wrapMode: Text.Wrap;

        anchors.left: column.left;
        
        color: "darkred";
        text: qsTr("Ip address and subnet mask cannot be changed for multiple servers");
        
        visible: !repeater.model.isSingleSelection;
    }
}

