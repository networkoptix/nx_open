import QtQuick 2.1;

import "../common" as Common;
import "../controls/base" as Base;
import "../controls/expandable" as Expandable;

import networkoptix.rtu 1.0 as NxRtu;

Expandable.GenericSettingsPanel
{
    property int flags: NxRtu.Constants.NoExtraFlags;

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
                enabled: NxRtu.Constants.OsRestartAbility;

                thin: true;
                height: Common.SizeManager.clickableSizes.medium;
                text: "Hardware Restart";
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
                enabled: NxRtu.Constants.ResetToFactoryDefaultsAbility;

                thin: true;
                height: Common.SizeManager.clickableSizes.medium;
                text: "Reset All";
            }

            Base.Button
            {
                enabled: NxRtu.Constants.ResetToFactoryDefaultsAbility;

                thin: true;
                height: Common.SizeManager.clickableSizes.medium;
                text: "Reset All But Network";
            }
        }

    }
}
