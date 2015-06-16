import QtQuick 2.1;

import "../common" as Common;
import "../controls/base" as Base;
import "../controls/expandable" as Expandable;

Expandable.MaskedSettingsPanel
{
    id: thisComponent;

    changed:  (maskedArea && maskedArea.changed?  true : false);
    
    function tryApplyChanges()
    {
        if (!changed)
            return true;
        return maskedArea.tryApplyChanges();
    }

    propertiesGroupName: qsTr("Set System Name and Password");

    propertiesDelegate: Component
    {
        id: updatePage;

        Item
        {
            property bool changed: systemName.changed || password.changed;
            height: settingsColumn.height;

            function tryApplyChanges()
            {
                if (systemName.changed)
                {
                    if (systemName.text.length === 0)
                    {
                        systemName.focus = true;
                        return false;
                    }

                    rtuContext.changesManager().addSystemChange(systemName.text);
                }
                if (password.changed)
                {
                    if (password.text.length === 0)
                    {
                        password.focus = true;
                        return false;
                    }

                    rtuContext.changesManager().addPasswordChange(password.text);
                }
                return true;
            }

            Base.Column
            {
                id: settingsColumn;

                anchors
                {
                    left: parent.left;
                    top: parent.top;
                    leftMargin: Common.SizeManager.spacing.base;
                }

                Base.Text
                {
                    text: qsTr("System Name");
                    font.pixelSize: Common.SizeManager.fontSizes.base;
                }

                Base.TextField
                {
                    id: systemName;
                    
                    revertOnEmpty: true;
                    implicitWidth: implicitHeight * 6;
                    initialText: (rtuContext.selection && rtuContext.selection !== null ?
                        rtuContext.selection.systemName : "");
                }

                Item
                {
                    id: spacer;
                    width: 1;
                    height: Common.SizeManager.spacing.base;
                }

                Base.Text
                {
                    text: qsTr("System Password");
                    font.pixelSize: Common.SizeManager.fontSizes.base;
                }

                Base.TextField
                {
                    id: password;

                    revertOnEmpty: true;
                    implicitWidth: systemName.implicitWidth;
                    initialText: rtuContext.selection.password;
                }
            }
        }
    }
}

