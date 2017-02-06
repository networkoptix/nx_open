import QtQuick 2.1;

import "../common" as Common;
import "../dialogs" as Dialogs;
import "../controls/base" as Base;
import "../controls/expandable" as Expandable;

import networkoptix.rtu 1.0 as NxRtu;

Expandable.GenericSettingsPanel
{
    id: thisComponent;

    property int flags: NxRtu.RestApiConstants.NoCommands;

    propertiesGroupName: qsTr("Actions");

    areaDelegate: Column
    {
        spacing: Common.SizeManager.spacing.base;

        anchors
        {
            left: parent.left;
            right: parent.right;
            leftMargin: Common.SizeManager.spacing.base;
            rightMargin: Common.SizeManager.spacing.base;
        }

        Base.Text
        {
            thin: false;
            text: "Restart";
            font.pixelSize: Common.SizeManager.fontSizes.medium;
        }

        Dialogs.MessageDialog
        {
            id: confirmationDialog;

            property var confirmedHandler;

            buttons: (NxRtu.Buttons.Yes | NxRtu.Buttons.No);
            styledButtons: NxRtu.Buttons.Yes;
            cancelButton: NxRtu.Buttons.No;

            onButtonClicked:
            {
                if ((id == NxRtu.Buttons.Yes) && confirmedHandler)
                    confirmedHandler();

                confirmedHandler = undefined;
            }
        }

        Row
        {
            spacing: Common.SizeManager.spacing.base;

            Base.Button
            {
                thin: true;
                height: Common.SizeManager.clickableSizes.medium;
                text: "Restart Server";

                onClicked:
                {
                    confirmationDialog.message = qsTr("Are you sure you want to restart the server(s)?");
                    confirmationDialog.confirmedHandler = function()
                    {
                        rtuContext.changesManager().changeset().addSoftRestartAction();
                        rtuContext.changesManager().applyChanges();
                    }
                    confirmationDialog.show();
                }
            }

            Base.Button
            {
                enabled: (rtuContext.selection.SystemCommands
                    & NxRtu.RestApiConstants.RebootCmd);

                thin: true;
                height: Common.SizeManager.clickableSizes.medium;
                text: "Reboot";

                onClicked:
                {
                    confirmationDialog.message = qsTr("Are you sure you want to reboot the server(s)?");
                    confirmationDialog.confirmedHandler = function()
                    {
                        rtuContext.changesManager().changeset().addOsRestartAction();
                        rtuContext.changesManager().applyChanges();
                    }
                    confirmationDialog.show();
                }
            }
        }

        Base.EmptyCell { }

        Base.Text
        {
            thin: false;
            text: "Restore server factory defaults";
            font.pixelSize: Common.SizeManager.fontSizes.medium;
        }

        Row
        {
            spacing: Common.SizeManager.spacing.base;

            Base.Button
            {
                enabled: ((rtuContext.selection.SystemCommands & NxRtu.RestApiConstants.FactoryDefaultsCmd)
                    && (rtuContext.selection.flags & NxRtu.RestApiConstants.HasHdd));

                thin: true;
                height: Common.SizeManager.clickableSizes.medium;
                text: "Full Restore";

                onClicked:
                {
                    confirmationDialog.message = qsTr("Are you sure you want to restore factory default settings on the selected server(s)?");
                    confirmationDialog.confirmedHandler = function()
                    {
                        rtuContext.changesManager().changeset().addFactoryDefaultsAction();
                        rtuContext.changesManager().applyChanges();
                    }
                    confirmationDialog.show();
                }
            }

            Base.Button
            {
                enabled: ((rtuContext.selection.SystemCommands & NxRtu.RestApiConstants.FactoryDefaultsButNetworkCmd)
                    && (rtuContext.selection.flags & NxRtu.RestApiConstants.HasHdd));

                thin: true;
                height: Common.SizeManager.clickableSizes.medium;
                text: "Restore Everything But Network";

                KeyNavigation.tab: thisComponent.nextTab;

                onClicked:
                {
                    confirmationDialog.message = qsTr("Are you sure you want to restore factory default settings on the selected server(s)?");
                    confirmationDialog.confirmedHandler = function()
                    {
                        rtuContext.changesManager().changeset().addFactoryDefaultsButNetworkAction();
                        rtuContext.changesManager().applyChanges();
                    }
                    confirmationDialog.show();
                }
            }
        }

    }
}
