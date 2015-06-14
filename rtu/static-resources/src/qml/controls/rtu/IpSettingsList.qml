import QtQuick 2.1;
import QtQuick.Controls 1.1;

import "." as Rtu;
import "../base" as Base;
import "../../common" as Common;

Base.Column
{
    id: thisComponent;

    property bool changed:
    {      
        var result = false;
        var children = column.children;
        for (var i = 0; i !== children.length; ++i)
        {
            var item = children[i];
            if (!item.hasOwnProperty("changed"))
                continue;
            
            result |= item.changed;
        }
        return result;
    }
    
    function applyButtonPressed()
    {
        if (!thisComponent.changed)
            return;

        var children = column.children;
        for (var i = 0; i !== children.length; ++i)
        {
            var item = children[i];
            if (!item.hasOwnProperty("adapterNameValue")|| !item.hasOwnProperty("isSingleSelectionModel") 
                || !item.hasOwnProperty("changed") || !item.hasOwnProperty("isSingleSelectionModel")
                || !item.changed)
            {
                continue;
            }
            
            if (!item.isSingleSelectionModel)
            {
                var forceUseDHCP = (item.useDHCPControl.checkedState !== Qt.Unchecked ? true : false);
                rtuContext.changesManager().turnOnDhcp();
                return;
            }

            var useDHCP = (item.useDHCPControl.checkedState !== Qt.Unchecked ? true : false);
            var ipAddress = (item.ipAddressControl.changed || !useDHCP ? item.ipAddressControl.text : "");
            var subnetMask = (item.subnetMaskControl.changed || !useDHCP ? item.subnetMaskControl.text : "");
            rtuContext.changesManager().addIpChange(
                item.adapterNameValue, useDHCP, ipAddress, subnetMask);
        }
    }
    
    Base.Column
    {
        id: column;

        anchors
        {
            left: parent.left;            
        }

        Repeater
        {
            id: repeater;
            
            model: rtuContext.ipSettingsModel();
            
            delegate: Rtu.IpChangeLine
            {
                isSingleSelectionModel: (repeater.model ? repeater.model.isSingleSelection : true);
                
                useDHCPControl.initialCheckedState: (isSingleSelectionModel ? useDHCP : false);
                useDHCPControl.text: (isSingleSelectionModel ? qsTr("Use DHCP") : qsTr("Force use DHCP on selection"));
                

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
    onWidthChanged: console.log("**width: " + width);
}

