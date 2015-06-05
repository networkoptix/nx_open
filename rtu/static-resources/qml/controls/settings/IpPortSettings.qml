import QtQuick 2.4;
import QtQuick.Controls 1.2;

import ".."
import "../standard" as Rtu;

MaskedSettingsPanel
{
    id: thisComponent;

    propertiesGroupName: qsTr("IP address and port");

    propertiesDelegate: Component
    {
        id: test;

        Row
        {
            spacing: SizeManager.spacing.large;
            
            height: ((ipList.height ? ipList.height : portColumn.height) + anchors.topMargin);
         
            anchors
            {
                leftMargin: SizeManager.spacing.base;
                topMargin: SizeManager.spacing.base;
            }

            IpSettingsList 
            {
                id: ipList;
            
                changesHandler: thisComponent;
            }
             
            Column
            {
                id: portColumn;
                         
                spacing: SizeManager.spacing.base;
                
                Rtu.Text
                {
                    text: qsTr("Port number:");
                }
                
                Rtu.TextField
                {
                    id: portNumber;
                    
                    changesHandler: thisComponent;
                    
                    width: SizeManager.clickableSizes.base * 2.5;
                    initialText: (rtuContext.selectionPort === 0 ? "" : rtuContext.selectionPort);
                }
            }
            
            Connections
            {
                target: thisComponent;
                
                onApplyButtonPressed:
                {
                    if (portNumber.changed)
                        rtuContext.changesManager().addPortChangeRequest(Number(portNumber.text));
                    
                    ipList.applyButtonPressed();       
                }
            }
        }
    }
}

