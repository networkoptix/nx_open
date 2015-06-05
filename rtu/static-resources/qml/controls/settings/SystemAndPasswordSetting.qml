import QtQuick 2.4;

import ".." as Common;
import "../standard" as Rtu;

Common.MaskedSettingsPanel
{
    id: thisComponent;

    propertiesGroupName: qsTr("System name and password");

    propertiesDelegate: Component
    {
        id: updatePage;

        Item
        {
            height: settingsColumn.height + settingsColumn.anchors.topMargin;

            Connections
            {
                target: thisComponent;
                onApplyButtonPressed:
                {
                    if (systemName.changed)
                        rtuContext.changesManager().addSystemChangeRequest(systemName.text);
                    if (password.changed)
                        rtuContext.changesManager().addPasswordChangeRequest(password.text);
                }
            }

            Column
            {
                id: settingsColumn;

                spacing: Common.SizeManager.spacing.base;
                anchors
                {
                    left: parent.left;
                    top: parent.top;
                    leftMargin: Common.SizeManager.spacing.base;
                    topMargin: Common.SizeManager.spacing.large;
                }

                Rtu.Text
                {
                    text: qsTr("System name");
                }

                Rtu.TextField
                {
                    id: systemName;
                    
                    initialText: rtuContext.selectionSystemName;
                    changesHandler: thisComponent;
                }

                Rtu.Text
                {
                    text: qsTr("New password");
                }

                Rtu.TextField
                {
                    id: password;

                    changesHandler: thisComponent;
                    initialText: rtuContext.selectionPassword;
                }
            }
        }
    }
}

