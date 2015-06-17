import QtQuick 2.1;
import QtQuick.Controls 1.1;

import "." as Rtu;
import "../base" as Base;
import "../../common" as Common;
import "../../dialogs" as Dialogs;

ListView
{
    id: thisComponent;

    property bool askForSelectionChange: false;

    clip: true;
    spacing: 1

    model: rtuContext.selectionModel();
    
    header: Rtu.SelectionHeader {}

    delegate: Loader
    {
        id: meatLoader;

        
        Component
        {
            id: systemDelegate;
            
            Rtu.SystemItemDelegate 
            {
                itemIndex: index;
                
                systemName: model.systemName;
                loggedState: model.loggedState;
                selectedState: model.selectedState;
            }
        }
    
        Component
        {
            id: serverDelegate;
            
            Rtu.ServerItemDelegate
            {
                logged: model.logged;
                serverName: model.name;
                macAddress: model.macAddress;
                selectedState: model.selectedState;
                
                onExplicitSelectionCalled: 
                {
                    impl.tryChangeSelectedServers(thisComponent.model.setItemSelected, index);
                }
            }
        }

        height: meatLoader.item.height;
        width: parent.width;

        sourceComponent: (model.isSystem ? systemDelegate : serverDelegate);
        Connections
        {
            target: meatLoader.item;
            onSelectionStateShouldBeChanged: 
            {
                impl.tryChangeSelectedServers(thisComponent.model.changeItemSelectedState, index);
            }
        }
    }

    Dialogs.ConfirmSelectionChangeDialog 
    {
        id: confirmationDialog;
    }

    property QtObject impl: QtObject
    {
        function tryChangeSelectedServers(changeFunc, index)
        {
            if (askForSelectionChange)
            {
                confirmationDialog.changeFunc = function() { changeFunc(index); }
                confirmationDialog.open();
            }
            else
            {
                changeFunc(index);
            }
        }
    }

}

