import QtQuick 2.0;

import "../../common" as Common;

import networkoptix.rtu 1.0 as NxRtu;

Item
{
    id: thisComponent;

    property int buttons: 0;
    property int styled: 0;
    
    property real buttonHeight: Common.SizeManager.clickableSizes.base;
    property real spacing: Common.SizeManager.spacing.base;

    signal buttonClicked(int id);
    
    
    width: row.width + row.anchors.rightMargin * 2;
    height: buttonHeight + spacing * 2;

    Row
    {
        id: row;
        
        spacing: thisComponent.spacing;
        anchors
        {
            right: parent.right;
            rightMargin: spacing;
            
            verticalCenter: parent.verticalCenter;
        }

        layoutDirection: Qt.RightToLeft;

        Repeater
        {
            model: NxRtu.Buttons 
            {
                buttons: thisComponent.buttons;
            }
            
            delegate: Loader
            {
                id: loader;
                
                anchors.verticalCenter: parent.verticalCenter;

                sourceComponent: (model.id & styled ? styledButton : button);
                
                onLoaded:
                {
                    item.height = thisComponent.buttonHeight;
                    item.width = item.height * model.aspect;
                    item.text = model.caption;
                }
                
                Connections
                {
                    target: loader.item;
                    onClicked: { thisComponent.buttonClicked(model.id); }
                }
            }
        }
    }

    Component
    {
        id: button;
        
        Button {}
    }

    Component
    {
        id: styledButton;

        StyledButton {}
    }

}
