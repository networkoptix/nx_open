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
        selectAllCheckedState: impl.selectAllCheckedState;
        
        onSelectAllServers: { impl.selectAllServers(select); }
    }

    footer: Rtu.SelectionFooter 
    {
        visible: thisComponent.contentHeight > thisComponent.height;

        selectAllCheckedState: impl.selectAllCheckedState;

        onSelectAllServers: { impl.selectAllServers(select); }
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

        Component
        {
            id: unknownGroupDeletate;

            Rtu.UnknownGroupDelegate
            {
                caption: model.name;
            }
        }

        Component
        {
            id: unknownItemDelegate;

            Rtu.UnknownItemDelegate
            {
                address: model.ipAddress;
            }
        }

        width: parent.width;

        sourceComponent:
        {
            switch(model.itemType)
            {
            case NxRtu.Constants.SystemItemType:
                return systemDelegate;
            case NxRtu.Constants.ServerItemType:
                return serverDelegate;
            case NxRtu.Constants.UnknownGroupType:
                return unknownGroupDeletate;
            case NxRtu.Constants.UnknownEntityType:
                return unknownItemDelegate;
            default:
                return undefined;
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
        readonly property int selectAllCheckedState:
            (!model.serversCount || !model.selectedCount ? Qt.Unchecked
            : (model.selectedCount === model.serversCount ? Qt.Checked : Qt.PartiallyChecked));

        function selectAllServers(select)
        {
            var selectedParam = select;
            impl.tryChangeSelectedServers(function()
            {
                thisComponent.model.setAllItemSelected(selectedParam);
            });
        }

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

    Dialogs.ErrorDialog
    {
        id: loginToSystemFailed;
        property string systemName;

        message: qsTr("Can't login to any server in system %1 with entered password").arg(systemName);

        Connections
        {
            target: rtuContext;
            onLoginOperationFailed:
            {
                loginToSystemFailed.systemName = primarySystem;
                loginToSystemFailed.show();
            }
        }
    }
}

