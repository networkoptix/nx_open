import QtQuick 2.1;

import "../common" as Common;
import "../controls/base" as Base;
import "../controls/expandable" as Expandable;
import "../dialogs" as Dialogs;

Expandable.MaskedSettingsPanel
{
    id: thisComponent;

    changed:  (maskedArea && maskedArea.changed?  true : false);

    function tryApplyChanges(warnings)
    {
        if (!changed)
            return true;
        return maskedArea.tryApplyChanges();
    }

    onMaskedAreaChanged:
    {
        if (warned)
            maskedArea.systemNameControl.forceActiveFocus();
    }

    propertiesGroupName: qsTr("Set System Name and Password");

    propertiesDelegate: FocusScope
    {
        property bool changed: systemName.changed || password.changed;
        property alias systemNameControl: systemName;

        height: settingsColumn.height;


        Dialogs.ErrorDialog
        {
            id: errorDialog;
        }

        readonly property string errorTemplate: qsTr("Invalid %1 specified. Can't apply changes");
        function tryApplyChanges()
        {
            if (systemName.changed)
            {
                if (systemName.text.trim().length === 0)
                {
                    errorDialog.message = errorTemplate.arg(qsTr("system name"));
                    errorDialog.show();

                    systemName.forceActiveFocus();
                    return false;
                }

                rtuContext.changesManager().changeset().addSystemChange(systemName.text);
            }
            if (password.changed)
            {
                if (password.text.trim().length === 0)
                {
                    errorDialog.message = errorTemplate.arg(qsTr("password"));
                    errorDialog.show();

                    password.forceActiveFocus();
                    return false;
                }

                rtuContext.changesManager().changeset().addPasswordChange(password.text);
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
                thin: false;
                text: qsTr("System Name");
            }

            Base.TextField
            {
                id: systemName;

                readonly property string kDifferentSystems: qsTr("<different systems>");

                implicitWidth: implicitHeight * 6;

                clearOnInitValue: ((rtuContext.selection.systemName.length === 0)
                    && rtuContext.selection.count !== 1);
                initialText: (clearOnInitValue ?
                    kDifferentSystems : rtuContext.selection.systemName);
            }

            Item
            {
                id: spacer;
                width: 1;
                height: Common.SizeManager.spacing.base;
            }

            Base.Text
            {
                thin: false;
                text: qsTr("System Password");
            }

            Base.TextField
            {
                id: password;

                KeyNavigation.backtab: systemName;
               // KeyNavigation.tab: password;

                readonly property string kDifferentPasswords: qsTr("<different passwords>");

                implicitWidth: systemName.implicitWidth;

                clearOnInitValue: (rtuContext.selection.password.length === 0);
                initialText: (clearOnInitValue ?
                    kDifferentPasswords : rtuContext.selection.password);
            }
        }
    }
}

