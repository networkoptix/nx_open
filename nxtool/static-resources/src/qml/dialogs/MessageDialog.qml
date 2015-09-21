import QtQuick 2.0;
import QtQuick.Window 2.0;

import "." as Dialogs;
import "../common" as Common;
import "../controls/base" as Base;

import networkoptix.rtu 1.0 as NxRtu;

Dialogs.Dialog
{
    id: thisComponent;

    property real maxMessageWidth: 400;
    property string message;
    
    buttons: NxRtu.Buttons.Ok;
    styledButtons: NxRtu.Buttons.Ok;
    cancelButton: NxRtu.Buttons.Ok;

    areaDelegate: Item
    {
    
        width: messageText.width + Common.SizeManager.spacing.base * 2;
        height: messageText.height + Common.SizeManager.spacing.base * 2;
        
        Base.Text
        {
            id: messageText;
    
            anchors.centerIn: parent;
            
            text: message;
            wrapMode: Text.Wrap;
            height: contentHeight;
            font.pixelSize: Common.SizeManager.fontSizes.medium;
            onWidthChanged:
            {
                if (width > thisComponent.maxMessageWidth)
                    width = thisComponent.maxMessageWidth;
            }
        }
    }


}
