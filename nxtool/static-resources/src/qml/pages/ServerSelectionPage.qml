import QtQuick 2.1;
import QtQuick.Controls 1.1;
import QtQuick.Dialogs 1.1;
import QtQuick.Layouts 1.0;

import "../common" as Common;
import "../controls/rtu" as Rtu;
import "../dialogs" as Dialogs;

import networkoptix.rtu 1.0 as NxRtu;

ScrollView
{
    id: thisComponent;

    function tryChangeSelection(func)
    {
        if (selectionView.askForSelectionChange)
        {
            confirmationDialog.changeSelectionFunc = function() { func(); };
            confirmationDialog.show();
        }
        else
        {
            func();
        }
    }

    signal applyChanges();

    property alias askForSelectionChange: selectionView.askForSelectionChange;


    Rtu.ServerSelectionListView
    {
        id: selectionView;

        anchors.fill: parent;
        
        onApplyChanges: { thisComponent.applyChanges(); }
        onTryChangeSelectedServers:
        {
            thisComponent.tryChangeSelection(func);
            rtuContext.hideProgressTask();
        }

        Dialogs.MessageDialog
        {
            id: confirmationDialog;

            property var changeSelectionFunc;

            buttons: (NxRtu.Buttons.ApplyChanges
                | NxRtu.Buttons.DiscardChanges | NxRtu.Buttons.Cancel);
            styledButtons: NxRtu.Buttons.ApplyChanges;
            cancelButton: NxRtu.Buttons.Cancel;
            
            title: qsTr("Confirmation");
            message: qsTr("Configuration changes have not been saved yet");

            onButtonClicked:
            {
                switch(id)
                {
                case NxRtu.Buttons.ApplyChanges:
                    thisComponent.applyChanges();
                    return;
                case NxRtu.Buttons.DiscardChanges:
                    if (changeSelectionFunc)
                        changeSelectionFunc();
                    return;
                }
            }
        }

        Component.onCompleted:
        {
            thisComponent.Layout.minimumHeight = selectionView.headerItem.height + Common.SizeManager.spacing.base;
        }
    }
}


