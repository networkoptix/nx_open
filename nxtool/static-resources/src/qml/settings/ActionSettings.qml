import QtQuick 2.1;

import "../common" as Common;
import "../controls/base" as Base;
import "../controls/expandable" as Expandable;

import networkoptix.rtu 1.0 as NxRtu;

Expandable.GenericSettingsPanel
{
    id: thisComponent;

    property int flags: NxRtu.Constants.NoCommands;

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
            text: "Restart server";
            font.pixelSize: Common.SizeManager.fontSizes.medium;
        }

        Row
        {
            spacing: Common.SizeManager.spacing.base;

            Base.Button
            {
                thin: true;
                height: Common.SizeManager.clickableSizes.medium;
                text: "Software Restart";

                onClicked:
                {
                    rtuContext.changesManager().changeset().addSoftRestartAction();
                    rtuContext.changesManager().applyChanges();
                }
            }

            Base.Button
            {
                enabled: (rtuContext.selection.SystemCommands
                    && NxRtu.Constants.RebootCmd);

                thin: true;
                height: Common.SizeManager.clickableSizes.medium;
                text: "Hardware Restart";

                onClicked:
                {
                    rtuContext.changesManager().changeset().addOsRestartAction();
                    rtuContext.changesManager().applyChanges();
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
                enabled: (rtuContext.selection.SystemCommands
                    && NxRtu.Constants.FactoryDefaultsCmd);

                thin: true;
                height: Common.SizeManager.clickableSizes.medium;
                text: "Reset All";

                onClicked:
                {
                    rtuContext.changesManager().changeset().addFactoryDefaultsAction();
                    rtuContext.changesManager().applyChanges();
                }
            }

            Base.Button
            {
                enabled: (rtuContext.selection.SystemCommands
                    && NxRtu.Constants.FactoryDefaultsButNetworkCmd);

                thin: true;
                height: Common.SizeManager.clickableSizes.medium;
                text: "Reset All But Network";

                KeyNavigation.tab: thisComponent.nextTab;

                onClicked:
                {
                    rtuContext.changesManager().changeset().addFactoryDefaultsButNetworkAction();
                    rtuContext.changesManager().applyChanges();
                }
            }
        }

    }
}
