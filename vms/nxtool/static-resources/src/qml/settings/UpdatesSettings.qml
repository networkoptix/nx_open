import QtQuick 2.1;

import ".." as Common;
import "../standard" as Rtu;

Common.GenericSettingsPanel
{
    id: thisComponent;

    propertiesGroupName: qsTr("Updates");

    areaDelegate: Component
    {
        id: updatePage;

        Item
        {
            height: settingsColumn.height + settingsColumn.anchors.topMargin + settingsColumn.anchors.bottomMargin;
            anchors
            {
                left: parent.left;
                right: parent.right;
                leftMargin: Common.SizeManager.spacing.base;
            }

            Column
            {
                id: settingsColumn;

                spacing: Common.SizeManager.spacing.base;
                anchors
                {
                    top: parent.top;
                    topMargin: Common.SizeManager.spacing.large;
                    bottomMargin: Common.SizeManager.spacing.base;
                }

                Text
                {
                    id: latestAvailableVersion;

                    text: qsTr("Latest available version: 2.4");
                    font.pointSize: Common.SizeManager.fontSizes.medium;
                }

                Row
                {
                    id: settingsRow;

                    spacing: Common.SizeManager.spacing.base;

                    Rtu.Button
                    {
                        height: Common.SizeManager.clickableSizes.medium;
                        text: qsTr("Check for updates");
                    }

                    Rtu.Button
                    {
                        height: Common.SizeManager.clickableSizes.medium;
                        text: qsTr("Update");
                    }
                }
            }
        }
    }
}

