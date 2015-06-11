import QtQuick 2.1;

import "../common" as Common;
import "../controls/base" as Base;
import "../controls/rtu" as Rtu;
Item
{
    id: thisComponent;
    
    Flickable
    {
        clip: true;
        anchors
        {
            left: parent.left;
            right: parent.right;
            top: parent.top;
            bottom: okButton.top;
            
            topMargin: Common.SizeManager.spacing.base;
            bottomMargin: Common.SizeManager.spacing.base;
        }

        flickableDirection: Flickable.VerticalFlick;
        contentWidth: content.width;
        contentHeight: content.height;
    
        Base.Column
        {
            id: content;
            
            width: thisComponent.width;
            spacing: Common.SizeManager.spacing.large;
            
            Rtu.ChangesSummary
            {
                changesCount: model.changesCount;
                captionTemplate: qsTr("%1 changes successfully applied");
                model: rtuContext.changesManager().successfulResultsModel();
            }
            
            Rtu.ChangesSummary
            {
                changesCount: model.changesCount;
                captionTemplate: qsTr("%1 errors");
                model: rtuContext.changesManager().failedResultsModel();
            }
        }
    }
    
    Base.Button
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
        onClicked:
        {
            rtuContext.currentPage = 0;
            rtuContext.changesManager().clearChanges();
        }
    }
}
