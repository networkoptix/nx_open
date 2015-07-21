import QtQuick 2.0;
import QtQuick.Dialogs 1.1;

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
