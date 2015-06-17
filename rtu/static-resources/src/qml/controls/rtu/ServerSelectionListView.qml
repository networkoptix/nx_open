import QtQuick 2.1;
import QtQuick.Controls 1.1;

import "." as Rtu;
import "../base" as Base;
import "../../common" as Common;
import "../../dialogs" as Dialogs;

import networkoptix.rtu 1.0 as NxRtu;

ListView
{
    id: thisComponent;

    signal applyChanges();
    
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

    Dialogs.MessageDialog 
    {
        id: confirmationDialog;
        
        property var changeSelectionFunc;
        property var applyChangesFunc;
        
        buttons: (NxRtu.Buttons.ApplyChanges 
            | NxRtu.Buttons.DiscardChanges | NxRtu.Buttons.Cancel);
        styledButtons: NxRtu.Buttons.ApplyChanges;
        
        title: qsTr("Confirmation");
        message: qsTr("Configuration changes have not been saved yet");
        
        onButtonClicked:
        {
            switch(id)
            {
            case NxRtu.Buttons.ApplyChanges:
                if (applyChangesFunc)
                    applyChangesFunc();
                return;
            case NxRtu.Buttons.DiscardChanges:
                if (changeSelectionFunc)
                    changeSelectionFunc();
                return;
            }
        }
    }

    property QtObject impl: QtObject
    {
        function tryChangeSelectedServers(changeFunc, index)
        {
            if (askForSelectionChange)
            {
                confirmationDialog.changeSelectionFunc = function() { changeFunc(index); }
                confirmationDialog.applyChangesFunc = thisComponent.applyChanges;
                confirmationDialog.show();
            }
            else
            {
                changeFunc(index);
            }
        }
    }

}

