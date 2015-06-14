import QtQuick 2.1;

import "../common" as Common;
import "../controls/base" as Base;
import "../controls/expandable" as Expandable;

Expandable.MaskedSettingsPanel
{
    id: thisComponent;

    changed:  (maskedArea && maskedArea.changed?  true : false);
    
    propertiesGroupName: qsTr("System name and password");

    propertiesDelegate: Component
    {
        id: updatePage;

        Item
        {
            property bool changed: systemName.changed || password.changed;
            height: settingsColumn.height + settingsColumn.anchors.topMargin;
            
            Connections
            {
                target: thisComponent;
                onApplyButtonPressed:
                {
                    if (systemName.changed)
                        rtuContext.changesManager().addSystemChange(systemName.text);
                    if (password.changed)
                        rtuContext.changesManager().addPasswordChange(password.text);
                }
            }

            Base.Column
            {
                id: settingsColumn;

                anchors
                {
                    left: parent.left;
                    top: parent.top;
                    leftMargin: Common.SizeManager.spacing.base;
                    topMargin: Common.SizeManager.spacing.large;
                }

                Base.Text
                {
                    text: qsTr("System name");
                }

                Base.TextField
                {
                    id: systemName;
                    
                    implicitWidth: implicitHeight * 6;

                    initialText: (rtuContext.selection && rtuContext.selection !== null ?
                        rtuContext.selection.systemName : "");
                }

                Base.Text
                {
                    text: qsTr("New password");
                }

                Base.TextField
                {
                    id: password;

                    implicitWidth: systemName.implicitWidth;

                    initialText: rtuContext.selection.password;
                }
            }
        }
    }
}

