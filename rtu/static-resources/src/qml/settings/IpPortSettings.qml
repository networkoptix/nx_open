import QtQuick 2.1;
import QtQuick.Controls 1.1;

import "../common" as Common;
import "../controls/rtu" as Rtu;
import "../controls/base" as Base;
import "../controls/expandable" as Expandable;

Expandable.MaskedSettingsPanel
{
    id: thisComponent;

    propertiesGroupName: qsTr("IP address and port");

    propertiesDelegate: Component
    {
        id: test;

        Row
        {
            spacing: Common.SizeManager.spacing.large;
            
            height: ((ipList.height ? ipList.height : portColumn.height) + anchors.topMargin);
         
            anchors
            {
                leftMargin: Common.SizeManager.spacing.base;
                topMargin: Common.SizeManager.spacing.base;
            }

            Rtu.IpSettingsList
            {
                id: ipList;
            
                changesHandler: thisComponent;
            }
             
            Base.Column
            {
                id: portColumn;
                         
                Base.Text
                {
                    text: qsTr("Port number:");
                }
                
                Base.TextField
                {
                    id: portNumber;
                    
                    changesHandler: thisComponent;
                    
                    width: Common.SizeManager.clickableSizes.base * 2.5;
                    initialText: rtuContext.selection.port;
                }
            }
            
            Connections
            {
                target: thisComponent;
                
                onApplyButtonPressed:
                {
                    if (portNumber.changed)
                        rtuContext.changesManager().addPortChange(Number(portNumber.text));
                    
                    ipList.applyButtonPressed();       
                }
            }
        }
    }
}

