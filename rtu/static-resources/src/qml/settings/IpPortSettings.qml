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

    propertiesDelegate: Component
    {
        id: test;

        Row
        {
            property bool changed: portNumber.changed || flagged.changed;
            
            spacing: Common.SizeManager.spacing.large;
            
            height: Math.max(flagged.height, portColumn.height) + anchors.topMargin;
         
            anchors
            {
                left: (parent ? parent.left : undefined);
                top: (parent ? parent.top : undefined);
                leftMargin: Common.SizeManager.spacing.base;
                topMargin: Common.SizeManager.spacing.base;
            }

            Rtu.FlaggedItem
            {
                id: flagged;

                property bool changed: (showFirst && currentItem.changed ? true : false);
                
                message: qsTr("Can not change interfaces settings for some selected servers");
                anchors
                {
                    top: (showFirst ? parent.top : undefined);
                    verticalCenter: (showFirst ? undefined : parent.verticalCenter);
                }
                showItem: ((Utils.Constants.AllowIfConfigFlag & rtuContext.selection.flags)
                    || (rtuContext.selection.count === 1));
                
                item: Rtu.IpSettingsList
                {
                    id: ipList;
                }
            }
            
            Base.Column
            {
                id: portColumn;
                
                anchors.top: parent.top;
                
                Base.Text
                {
                    text: qsTr("Port:");
                }
                
                Base.PortControl
                {
                    id: portNumber;
                    
                    width: Common.SizeManager.clickableSizes.base * 3.5;
                    initialPort: rtuContext.selection.port;
                }
            }
            
            Connections
            {
                target: thisComponent;
                
                onApplyButtonPressed:
                {
                    if (portNumber.changed)
                        rtuContext.changesManager().addPortChange(Number(portNumber.text));
                    
                    if (flagged.showFirst)
                        flagged.currentItem.applyButtonPressed();       
                }
            }
        }
    }
}

