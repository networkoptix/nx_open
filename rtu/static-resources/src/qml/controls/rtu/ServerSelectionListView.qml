import QtQuick 2.1;
import QtQuick.Controls 1.1;
import QtQuick.Dialogs 1.1;

import "." as Rtu;
import "../../common" as Common;

ListView
{
    id: thisComponent;

    property bool askForSelectionChanges;

    onCountChanged: console.log(count);


    clip: true;
    spacing: Common.SizeManager.spacing.small;

    model: rtuContext.selectionModel();
    
    delegate: Loader
    {
        id: meatLoader;

        
        Component
        {
            id: systemDelegate;
            
            Rtu.SystemItemDelegate {}
        }
    
        Component
        {
            id: serverDelegate;
            
            Rtu.ServerItemDelegate
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

