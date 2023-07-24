// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15

import Nx 1.0
import Nx.Core 1.0
import Nx.Controls 1.0
import Nx.Dialogs 1.0

import nx.vms.client.desktop 1.0

import "../UserManagement/Components"
import "ldap_helper.mjs" as LdapHelper

Dialog
{
    id: dialog

    modality: Qt.ApplicationModal

    minimumWidth: 700
    minimumHeight: content.height + buttonBox.implicitHeight + 32
    maximumHeight: minimumHeight

    width: minimumWidth
    height: minimumHeight

    property string uri
    onUriChanged: ldapUri.text = uri

    property alias adminDn: adminDnTextField.text
    property string originalAdminDn: ""
    property alias password: passwordTextField.text
    property alias useStarTls: starTlsCheckbox.checked
    property alias ignoreCertErrors: ignoreCertErrorsCheckbox.checked
    property bool showFakePassword: false

    property var self
    property var testState: LdapSettings.TestState.initial
    property string testMessage: ""

    readonly property alias ldapScheme: schemeCombobox.currentValue
    readonly property alias hostAndPort: ldapUri.text

    title: qsTr("LDAP - Connection Settings")

    color: ColorTheme.colors.dark7

    function openNew()
    {
        dialog.show()
        dialog.raise()
    }

    Column
    {
        id: content

        anchors.top: parent.top
        anchors.topMargin: 16
        anchors.left: parent.left
        anchors.leftMargin: 16
        anchors.right: parent.right
        anchors.rightMargin: 16

        spacing: 8

        CenteredField
        {
            text: qsTr("Host")

            leftSideMargin: 78
            rightSideMargin: 0

            RowLayout
            {
                width: parent.width

                ComboBox
                {
                    id: schemeCombobox

                    Layout.maximumWidth: 84
                    Layout.alignment: Qt.AlignTop
                    model: LdapHelper.kSchemes

                    readonly property bool supportsStarTls: LdapHelper.supportsStarTls(currentText)
                }

                TextFieldWithValidator
                {
                    id: ldapUri

                    focus: true

                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignTop

                    validateFunc: (text) =>
                    {
                        return text == ""
                            ? qsTr("This field cannot be empty")
                            : ""
                    }

                    onTextChanged:
                    {
                        const result = LdapHelper.splitUrl(text)
                        if (text === result.hostAndPort)
                            return

                        const prevPosition = cursorPosition

                        text = result.hostAndPort
                        cursorPosition = Math.max(prevPosition - result.scheme.length, 0)

                        schemeCombobox.currentIndex = schemeCombobox.model[1] == result.scheme
                            ? 1
                            : 0
                    }
                }
            }
        }

        CenteredField
        {
            text: qsTr("Login DN")

            leftSideMargin: 78
            rightSideMargin: 0

            TextFieldWithValidator
            {
                id: adminDnTextField

                width: parent.width

                validateFunc: (text) =>
                {
                    return text
                        ? ""
                        : qsTr("Login DN cannot be empty.")
                }
            }
        }

        CenteredField
        {
            text: qsTr("Password")

            leftSideMargin: 78
            rightSideMargin: 0

            PasswordFieldWithWarning
            {
                id: passwordTextField

                showFakePassword: dialog.showFakePassword
                    && dialog.originalAdminDn == dialog.adminDn

                showStrength: false
                width: parent.width

                validateFunc: (text) =>
                {
                    return passwordTextField.text || passwordTextField.showFakePassword
                        ? ""
                        : qsTr("Password cannot be empty.")
                }
            }
        }

        CenteredField
        {
            leftSideMargin: 78
            rightSideMargin: 0

            ColumnLayout
            {
                x: -4
                spacing: 3
                width: parent.width

                CheckBox
                {
                    id: starTlsCheckbox
                    text: qsTr("Use StartTLS")
                    wrapMode: Text.WordWrap

                    property bool prevChecked: false

                    enabled: schemeCombobox.supportsStarTls
                    onEnabledChanged:
                    {
                        // Save checked state when this checkbox is disabled.
                        if (!enabled)
                        {
                            prevChecked = checked
                            checked = false
                        }
                        else
                        {
                            checked = prevChecked
                        }
                    }
                }

                CheckBox
                {
                    id: ignoreCertErrorsCheckbox
                    text: qsTr("Ignore LDAP server certificate errors")
                    wrapMode: Text.WordWrap
                }
            }
        }

        CenteredField
        {
            leftSideMargin: 78
            rightSideMargin: 0

            RowLayout
            {
                width: parent.width

                baselineOffset: testButton.baselineOffset

                Button
                {
                    id: testButton

                    Layout.alignment: Qt.AlignTop | Qt.AlignLeft
                    Layout.minimumWidth: 80
                    text: qsTr("Test")

                    enabled: dialog.testState != LdapSettings.TestState.connecting

                    onClicked:
                    {
                        if (dialog.validate())
                        {
                            self.testConnection(
                                dialog.ldapScheme + dialog.hostAndPort,
                                dialog.adminDn,
                                dialog.password)
                        }
                    }
                }

                Image
                {
                    visible: dialog.testState == LdapSettings.TestState.ok
                        || dialog.testState == LdapSettings.TestState.error

                    Layout.alignment: Qt.AlignTop
                    Layout.topMargin: (testButton.height - height) / 2
                    Layout.leftMargin: 8

                    source: dialog.testState == LdapSettings.TestState.ok
                        ? "image://svg/skin/user_settings/connection_ok.svg"
                        : "image://svg/skin/user_settings/connection_error.svg"
                    sourceSize: Qt.size(20, 20)
                }

                Spinner
                {
                    Layout.alignment: Qt.AlignTop
                    Layout.topMargin: (testButton.height - height) / 2
                    Layout.leftMargin: 8
                    running: dialog.testState == LdapSettings.TestState.connecting
                }

                Text
                {
                    visible: dialog.testState != LdapSettings.TestState.initial

                    Layout.alignment: Qt.AlignTop
                    Layout.topMargin: testButton.baselineOffset - baselineOffset
                    Layout.fillWidth: true

                    text: dialog.testState == LdapSettings.TestState.connecting
                        ? qsTr("Connecting...")
                        : dialog.testMessage

                    color:
                    {
                        switch (dialog.testState)
                        {
                            case LdapSettings.TestState.error:
                                return ColorTheme.colors.red_core
                            case LdapSettings.TestState.ok:
                                return ColorTheme.colors.green_core
                            default:
                                return ColorTheme.colors.light16
                        }
                    }
                    wrapMode: Text.WordWrap
                }
            }
        }
    }

    function validate()
    {
        return ldapUri.validate()
            && adminDnTextField.validate()
            && passwordTextField.validate()
    }

    function accept()
    {
        if (validate())
        {
            dialog.accepted()
            dialog.close()
        }
    }

    function reject()
    {
        if (dialog.testState == LdapSettings.TestState.connecting)
            self.cancelCurrentRequest()

        dialog.rejected()
        dialog.close()
    }

    buttonBox: DialogButtonBox
    {
        id: buttonBox
        buttonLayout: DialogButtonBox.KdeLayout

        standardButtons: DialogButtonBox.Ok | DialogButtonBox.Cancel
    }
}
