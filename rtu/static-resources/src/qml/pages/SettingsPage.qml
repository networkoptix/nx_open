import QtQuick 2.1;

import "../common" as Common
import "../settings" as Settings;
import "../controls/base" as Base;

Item
{
    property bool parametersChanged: false;
    property var selectedServersModel;

    anchors.fill: parent;

    Flickable
    {
        clip: true;
        anchors
        {
            left: parent.left;
            right: parent.right;
            top: parent.top;
            bottom: buttonsPanel.top;
        }

        flickableDirection: Flickable.VerticalFlick;
        contentHeight: settingsColumn.height + settingsColumn.anchors.topMargin
            + settingsColumn.anchors.bottomMargin;

        Column
        {
            id: settingsColumn;

            spacing: Common.SizeManager.spacing.base;
            anchors
            {
                left: parent.left;
                right: parent.right;
                top: parent.top;

                topMargin: Common.SizeManager.fontSizes.base;
                bottomMargin: Common.SizeManager.fontSizes.base;
            }

            Settings.IpPortSettings
            {
                id: ipPortSettings;
            }

            Settings.DateTimeSettings
            {
                id: dateTimeSettings;
            }

            Settings.SystemAndPasswordSetting
            {
                id: systemAndPasswordSettings;
            }

/*
            UpdatesSettings
            {
             //   selectedServersModel: thisComponent.selectedServersModel;
            }
*/
        }
    }

    Item
    {
        id: buttonsPanel;

        height: Common.SizeManager.clickableSizes.large;
        anchors
        {
            left: parent.left;
            right: parent.right;
            bottom: parent.bottom;
        }

        Row
        {
            id: buttonsRow;

            anchors
            {
                left: parent.left;
                right: parent.right;
                leftMargin: Common.SizeManager.spacing.base;
                rightMargin: Common.SizeManager.spacing.base;
            }

            spacing: Common.SizeManager.spacing.base;
            layoutDirection: Qt.RightToLeft;

            Base.Button
            {
                id: applyButton;
                text: "Apply changes";

                enabled: systemAndPasswordSettings.changed || ipPortSettings.changed || dateTimeSettings.changed;
                opacity: (enabled ? 1.0 : 0.5);
                onClicked:
                {
                    ipPortSettings.applyButtonPressed();
                    systemAndPasswordSettings.applyButtonPressed();
                    dateTimeSettings.applyButtonPressed();

                    rtuContext.changesManager().applyChanges();
                }
            }

            Base.Button
            {
                text: "Cancel";

                enabled: applyButton.enabled;
            }
        }

    }
}

