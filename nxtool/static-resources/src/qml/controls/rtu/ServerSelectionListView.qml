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

    cacheBuffer: 65535;
    clip: true;
    spacing: 1

    model: rtuContext.selectionModel();
    
    header: Rtu.SelectionHeader 
    {
        selectAllCheckedState: (!model.serversCount || !model.selectedCount ? Qt.Unchecked
            : (model.selectedCount === model.serversCount ? Qt.Checked : Qt.PartiallyChecked));
        
        onSelectAllServers:
        {
            var selectedParam = select;
            impl.tryChangeSelectedServers(function()
            {
                thisComponent.model.setAllItemSelected(selectedParam);
            });
        }
    }

    footer: Rtu.SelectionFooter 
    {
        enabled: (model.serversCount && (model.selectedCount !== model.serversCount));
    
        onSelectAllClicked: impl.tryChangeSelectedServers(function()
        {
            thisComponent.model.setAllItemSelected(true);
        });
       
    }
    
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
                    var selectedIndex = index;
                    impl.tryChangeSelectedServers(function() 
                    {
                        thisComponent.model.setItemSelected(index);
                    });
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
                var selectedIndex = index;
                impl.tryChangeSelectedServers( function() 
                {
                    thisComponent.model.changeItemSelectedState(index);
                });
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
        function tryChangeSelectedServers(changeFunc)
        {
            if (askForSelectionChange)
            {
                confirmationDialog.changeSelectionFunc = function() { changeFunc(); }
                confirmationDialog.applyChangesFunc = thisComponent.applyChanges;
                confirmationDialog.show();
            }
            else
            {
                changeFunc();
            }
        }
    }

}

