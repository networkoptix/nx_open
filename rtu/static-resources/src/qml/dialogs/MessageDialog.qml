import QtQuick 2.0;
import QtQuick.Window 2.0;

import "../common" as Common;
import "../controls/base" as Base;
import "../controls/rtu" as Rtu;

Window
{
    id: thisComponent;

    property real maxMessageWidth: 400;
    property int buttons: 0;
    property int styledButtons: 0;
    property string message;

    signal buttonClicked(int id);
    
    function show()
    {
        visible = true;
    }

    title: "";
    flags: Qt.WindowTitleHint | Qt.MSWindowsFixedSizeDialogHint;
    modality: Qt.WindowModal;
    
    height: messageText.height + messageText.anchors.topMargin
        + buttonsPanel.height;
    width: Math.max(buttonsPanel.width
        , messageText.width + Common.SizeManager.spacing.medium * 2);

    Base.Text
    {
        id: messageText;

        anchors
        {
            top: parent.top;
            topMargin: Common.SizeManager.spacing.medium;
            horizontalCenter: parent.horizontalCenter;
        }
        
        
        text: message;
        wrapMode: Text.Wrap;
        height: contentHeight;
        
        onWidthChanged:
        {
            if (width > thisComponent.maxMessageWidth)
                width = thisComponent.maxMessageWidth;
        }
    }
    
    Base.ButtonsPanel
    {
        id: buttonsPanel;
        
        buttons: thisComponent.buttons;
        styled: thisComponent.styledButtons;
        
        y: thisComponent.height - height;
        anchors.right: parent.right;
        
        onButtonClicked: 
        {
            thisComponent.close();
            thisComponent.buttonClicked(id); 
        }
    }
}
