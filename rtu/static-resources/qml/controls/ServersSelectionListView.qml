import QtQuick 2.4;
import QtQuick.Controls 1.3;
import QtQuick.Dialogs 1.2;

import "../controls";

ListView
{
    id: thisComponent;

    property bool askForSelectionChanges;
    
    clip: true;
    spacing: SizeManager.spacing.small;

    model: rtuContext.selectionModel();
    
    delegate: Loader
    {
        id: meatLoader;

        
        Component
        {
            id: systemDelegate;
            
            SystemItemDelegate {}
        }
    
        Component
        {
            id: serverDelegate;
            
            ServerItemDelegate
            {
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

    MessageDialog
    {
        id: confirmationDialog;
        
        property var changeFunc: function() {}
        
        standardButtons: StandardButton.Yes | StandardButton.No;

        title: qsTr("Confirmation");
        text: qsTr("Do you want to select another server(s) and discard all changes of the setting?");
        informativeText: qsTr("You have changed one or more parameters of previously selected server(s)\
 and haven't applied them yet. All not applied changes will be lost.");
        
        onYes: changeFunc();
    }
    
    property QtObject impl: QtObject
    {
        function tryChangeSelectedServers(changeFunc, index)
        {
            if (askForSelectionChanges)
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

