import QtQuick 2.1;

import "../common" as Common;
import "../controls/base" as Base;
import "../controls/expandable" as Expandable;
import "../dialogs" as Dialogs;

Expandable.MaskedSettingsPanel
{
    id: thisComponent;

    extraWarned: rtuContext.selection.safeMode;

    changed:  d.passwordChanged || d.systemNameChanged

    extraInformation: rtuContext.selection.count > 1
        && (d.bothChanged || (rtuContext.selection.isNewSystem && d.appropriateFieldsForNewSystem))
        ? qsTr("Selected servers will be merged into single system")
        : ""

    readonly property string newSystemExplanationMessage:
    {
        var primaryMessage = rtuContext.selection.count == 1
            ? qsTr("Selected server is new and should be configured.")
            : qsTr("Some of selected servers are new and should be configured.");

        return primaryMessage + qsTr("\nPlease fill in System Name and System Password fields.");
    }

    function tryApplyChanges(warnings)
    {
        if (!changed)
            return true;
        return maskedArea.tryApplyChanges();
    }

    onMaskedAreaChanged:
    {
        if (warned && maskedArea)
            maskedArea.systemNameControl.forceActiveFocus();
    }

    propertiesGroupName: qsTr("Set System Name and Password");

    propertiesDelegate: FocusScope
    {
        property alias systemNameControl: systemName
        property alias passwordControl: password

        height: settingsColumn.height;

        Dialogs.ErrorDialog
        {
            id: errorDialog;
        }
        property bool isDifferentSystemNames:
            rtuContext.selection.systemName.length == 0 && rtuContext.selection.count !== 1

        function showError(message)
        {
            errorDialog.message = message;
            errorDialog.show();

            systemName.forceActiveFocus();
            return false;
        }

        function tryApplyChanges()
        {
            if (rtuContext.selection.isNewSystem)
            {
                if (!d.appropriateFieldsForNewSystem)
                    return showError(thisComponent.newSystemExplanationMessage);

                rtuContext.changesManager().changeset().addSystemChange(systemName.text);
                rtuContext.changesManager().changeset().addPasswordChange(password.text);
                return true;
            }

            if (systemName.changed)
            {
                if (systemName.text.trim().length === 0)
                    return showError(qsTr("Invalid system name specified. Can't apply changes"));

                rtuContext.changesManager().changeset().addSystemChange(systemName.text);
            }

            if (password.changed)
            {
                if (password.text.trim().length === 0)
                    return showError(qsTr("Invalid password specified. Can't apply changes"));

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

                clearOnInitValue: d.differentSystemNames;
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

                readonly property string kDifferentPasswords: qsTr("<different passwords>");

                KeyNavigation.backtab: systemName;
                implicitWidth: systemName.implicitWidth;

                clearOnInitValue: d.differentPasswords;
                initialText: (clearOnInitValue ?
                    kDifferentPasswords : rtuContext.selection.password);
            }
        }
    }

    QtObject
    {
        id: d

        readonly property bool systemNameChanged: maskedArea && maskedArea.systemNameControl
            ? maskedArea.systemNameControl.changed
            : false
        readonly property bool passwordChanged: maskedArea && maskedArea.passwordControl
            ? maskedArea.passwordControl.changed
            : false
        readonly property bool bothChanged: systemNameChanged && passwordChanged

        readonly property bool appropriateFieldsForNewSystem:
            appropriatePasswordForNewSystem && appropriateSystemNameForNewSystem

        readonly property bool nonEmptySystemName: maskedArea && maskedArea.systemNameControl
            ? maskedArea.systemNameControl.length > 0
            : false
        readonly property bool differentSystemNames:
            rtuContext.selection.systemName.length === 0 && rtuContext.selection.count !== 1
        readonly property bool appropriateSystemNameForNewSystem:
            (systemNameChanged || !differentSystemNames) && nonEmptySystemName

        readonly property bool nonEmptyPassword: maskedArea && maskedArea.passwordControl
            ? maskedArea.passwordControl.length > 0
            : false
        readonly property bool differentPasswords: rtuContext.selection.password.length == 0
        property bool appropriatePasswordForNewSystem:
            (passwordChanged || !differentPasswords) && nonEmptyPassword
    }
}


