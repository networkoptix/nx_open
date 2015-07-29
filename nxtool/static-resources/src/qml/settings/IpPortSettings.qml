import QtQuick 2.1;
import QtQuick.Controls 1.1;

import "../common" as Common;
import "../controls/rtu" as Rtu;
import "../controls/base" as Base;
import "../controls/expandable" as Expandable;

import networkoptix.rtu 1.0 as Utils;

Expandable.MaskedSettingsPanel
{
    id: thisComponent;
    
    changed:  (maskedArea && maskedArea.changed?  true : false);

    propertiesGroupName: qsTr("Configure Device Network Settings");

    function tryApplyChanges()
    {
        if (!changed)
            return true;
        
        return maskedArea.tryApplyChanges();
    }

    propertiesDelegate: Component
    {
        id: test;

        Row
        {
            function tryApplyChanges()
            {
                if (portNumber.changed)
                    rtuContext.changesManager().addPortChange(Number(portNumber.text));
                    
                if (!flagged.showFirst) // No changes 
                    return true;
                
                if (flagged.dhcpForceOnly && flagged.currentItem.forceUseDHCP)
                {
                    rtuContext.changesManager().turnOnDhcp();
                    return true;
                }

                return (flagged.currentItem.item.tryApplyChanges());
            }

            property bool changed: portNumber.changed || flagged.changed;
            
            spacing: Common.SizeManager.spacing.large;
            
            height: Math.max(flagged.height, portColumn.height);
         
            anchors
            {
                left: (parent ? parent.left : undefined);
                top: (parent ? parent.top : undefined);
                leftMargin: Common.SizeManager.spacing.base;
            }

            Rtu.FlaggedItem
            {
                id: flagged;

                property bool changed: (showFirst && currentItem.item 
                    && currentItem.item.changed ? true : false);
                property bool dhcpForceOnly: (rtuContext.selection.count !== 1);
                
                anchors
                {
                   top: (showFirst ? parent.top : undefined);
                   verticalCenter: (showFirst ? undefined : parent.verticalCenter);
                }
                
                message: qsTr("Can not change interfaces settings for some selected servers");
                showItem: ((Utils.Constants.AllowIfConfigFlag & rtuContext.selection.flags)
                    || (rtuContext.selection.count === 1));
                
                Component
                {
                    id: singleSelectionInterfaceList;
                        
                    Rtu.IpSettingsList
                    {
                        enabled: (Utils.Constants.AllowIfConfigFlag & rtuContext.selection.flags);
                    }
                }

                Component
                {
                    id: multipleSelectionInterfaceList;
                    
                    Rtu.MultipleSelectionInterfaceList
                    {
                        dhcpState: rtuContext.selection.dhcpState;
                    }
                }
                
                item: Loader
                {
                    id: loader;
                    
                    sourceComponent: (flagged.dhcpForceOnly ? 
                        multipleSelectionInterfaceList : singleSelectionInterfaceList);
                }
            }
        
            Base.Column
            {
                id: portColumn;
                
                Base.Text
                {
                    thin: false;
                    text: qsTr("Port");
                }
                
                Base.PortControl
                {
                    id: portNumber;
                    
                    width: Common.SizeManager.clickableSizes.base * 3;
                    initialPort: rtuContext.selection.port;
                }
            }
        }
    }
}

