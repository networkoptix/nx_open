import QtQuick 2.4;

import "../controls" as Common;
import "../controls/standard" as Rtu;

Item
{
    id: thisComponent;
    
    Component.onCompleted: { console.log("---------------- ") }
    
    Flickable
    {
        clip: true;
        flickableDirection: Flickable.VerticalFlick;
        
        anchors
        {
            left: parent.left;
            right: parent.right;
            top: parent.top;
            bottom: okButton.top;
            
            topMargin: Common.SizeManager.spacing.base;
            bottomMargin: Common.SizeManager.spacing.base;
        }

        contentWidth: content.width;
        contentHeight: content.height;
    
        Column
        {
            id: content;
            
            width: thisComponent.width;
            spacing: Common.SizeManager.spacing.large;
            
            ChangesSummary
            {
                changesCount: rtuContext.changesManager().totalChangesCount
                    - rtuContext.changesManager().errorsCount;
                captionTemplate: qsTr("%1 changes successfully applied");
                model: rtuContext.changesManager().successfulResultsModel();
                
            }
            
            ChangesSummary
            {
                changesCount: rtuContext.changesManager().errorsCount;
                captionTemplate: qsTr("%1 errors");
                model: rtuContext.changesManager().failedResultsModel();
            }
        }
    }
    
    Rtu.Button
    {
        id: okButton;
        
        anchors
        {
            right: parent.right;
            bottom: parent.bottom;
            
            rightMargin: Common.SizeManager.spacing.base;
            bottomMargin: Common.SizeManager.spacing.base;
        }

        text: qsTr("Ok");
        onClicked: { rtuContext.currentPage = 0; }
    }
}
