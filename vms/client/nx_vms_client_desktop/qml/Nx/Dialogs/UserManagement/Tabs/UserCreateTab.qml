// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import QtQuick.Shapes 1.15

import Qt5Compat.GraphicalEffects

import Nx 1.0
import Nx.Core 1.0
import Nx.Controls 1.0

import nx.vms.client.desktop 1.0

import "../Components"

Item
{
    id: control

    property alias userEnabled: enabledUserSwitch.checked
    property alias login: loginTextField.text
    property alias fullName: fullNameTextField.text
    property alias email: emailTextField.text
    property alias password: passwordTextField.text
    property alias allowInsecure: allowInsecureCheckBox.checked
    property alias linkValidUntil: temporaryLinkSettings.linkValidUntil
    property alias expiresAfterLoginS: temporaryLinkSettings.expiresAfterLoginS
    property alias revokeAccessEnabled: temporaryLinkSettings.revokeAccessEnabled
    property alias displayOffsetMs: temporaryLinkSettings.displayOffsetMs
    property alias loginText: loginTextField

    property int userType: UserSettingsGlobal.LocalUser

    property var self
    property alias model: groupsComboBox.model

    function validate()
    {
        switch (userType)
        {
            case UserSettingsGlobal.LocalUser:
                return loginTextField.validate()
                    && emailTextField.validate()
                    && passwordTextField.validate()
                    && passwordTextField.strengthAccepted
                    && confirmPasswordTextField.validate()

            case UserSettingsGlobal.TemporaryUser:
                return loginTextField.validate()
                    && emailTextField.validate()

            case UserSettingsGlobal.CloudUser:
                return emailTextField.validate()
        }

        return true
    }

    Scrollable
    {
        id: scroll
        anchors.fill: parent

        contentItem: Column
        {
            spacing: 8

            x: 16
            width: scroll.width - 16 * 2

            component Spacer: Item
            {
                width: 1
                height: 8
            }

            SectionHeader
            {
                text: qsTr("New User")
            }

            CenteredField
            {
                UserEnabledSwitch
                {
                    height: 28
                    id: enabledUserSwitch
                }
            }

            CenteredField
            {
                id: typeField

                text: qsTr("Type")

                visible: control.self ? control.self.isConnectedToCloud() : false

                ButtonGroup
                {
                    buttons: typeButtonsRow.children
                    exclusive: true
                    onCheckedButtonChanged:
                    {
                        control.userType = localUserButton.checked
                            ? (accessComboBox.currentIndex == 0
                                ? UserSettingsGlobal.LocalUser
                                : UserSettingsGlobal.TemporaryUser)
                            : UserSettingsGlobal.CloudUser
                    }
                }

                Row
                {
                    id: typeButtonsRow

                    spacing: 0

                    ToggleButton
                    {
                        id: localUserButton
                        text: qsTr("Local")
                        icon.source: "image://svg/skin/user_settings/toggle_local_user.svg"
                        flatSide: -1 //< Rigth.
                        checked: control.userType == UserSettingsGlobal.LocalUser
                            || control.userType == UserSettingsGlobal.TemporaryUser
                    }

                    ToggleButton
                    {
                        text: qsTr("Cloud")
                        icon.source: "image://svg/skin/user_settings/toggle_cloud_user.svg"
                        flatSide: 1 //< Left.
                        checked: control.userType == UserSettingsGlobal.CloudUser
                    }
                }
            }

            Spacer
            {
                visible: typeField.visible
            }

            CenteredField
            {
                visible: control.userType != UserSettingsGlobal.CloudUser

                text: qsTr("Login")

                TextFieldWithValidator
                {
                    id: loginTextField
                    width: parent.width
                    validateFunc: control.self ? (text) => control.self.validateLogin(text) : null
                    focus: control.userType != UserSettingsGlobal.CloudUser
                }
            }

            CenteredField
            {
                visible: control.userType != UserSettingsGlobal.CloudUser

                text: qsTr("Full Name")

                TextField
                {
                    id: fullNameTextField
                    width: parent.width
                }
            }

            CenteredField
            {
                text: qsTr("Email")

                TextFieldWithValidator
                {
                    id: emailTextField
                    focus: control.userType == UserSettingsGlobal.CloudUser
                    width: parent.width

                    readonly property bool isLocalUser:
                        control.userType == UserSettingsGlobal.LocalUser

                    onIsLocalUserChanged:
                    {
                        // Hide empty email warning when switching user type cloud -> local.
                        if (isLocalUser && !text)
                            textField.warningState = false
                    }

                    validateFunc: (text) =>
                    {
                        return control.self
                            ? control.self.validateEmail(
                                text, control.userType == UserSettingsGlobal.CloudUser)
                            : ""
                    }
                }
            }

            Spacer {}

            CenteredField
            {
                visible: control.userType == UserSettingsGlobal.LocalUser
                    || control.userType == UserSettingsGlobal.TemporaryUser

                text: qsTr("Access")

                ComboBox
                {
                    id: accessComboBox
                    width: parent.width
                    textRole: "text"
                    valueRole: "value"

                    model:
                    [
                        {
                            text: qsTr("Regular user with credentials"),
                            value: UserSettingsGlobal.LocalUser
                        },
                        {
                            text: qsTr("Temporary with link"),
                            value: UserSettingsGlobal.TemporaryUser
                        },
                    ]

                    onCurrentValueChanged:
                    {
                        if (control.userType == UserSettingsGlobal.LocalUser
                            || control.userType == UserSettingsGlobal.TemporaryUser)
                        {
                            control.userType = accessComboBox.currentValue
                        }
                    }
                }
            }

            TemporaryLinkSettings
            {
                id: temporaryLinkSettings

                visible: control.userType == UserSettingsGlobal.TemporaryUser
            }

            CenteredField
            {
                visible: control.userType != UserSettingsGlobal.CloudUser
                    && control.userType != UserSettingsGlobal.TemporaryUser

                text: qsTr("Password")

                PasswordFieldWithWarning
                {
                    id: passwordTextField
                    width: parent.width
                    validateFunc: (text) => UserSettingsGlobal.validatePassword(text)
                    onTextChanged: confirmPasswordTextField.warningState = false
                }
            }

            CenteredField
            {
                visible: control.userType != UserSettingsGlobal.CloudUser
                    && control.userType != UserSettingsGlobal.TemporaryUser

                text: qsTr("Confirm Password")

                PasswordFieldWithWarning
                {
                    id: confirmPasswordTextField
                    width: parent.width
                    showStrength: false
                    validateFunc: (text) =>
                    {
                        return passwordTextField.text == confirmPasswordTextField.text
                            ? ""
                            : qsTr("Passwords do not match.")
                    }
                }
            }

            CenteredField
            {
                visible: control.userType != UserSettingsGlobal.CloudUser
                    && control.userType != UserSettingsGlobal.TemporaryUser

                CheckBox
                {
                    id: allowInsecureCheckBox
                    text: qsTr("Allow insecure (digest) authentication")

                    font.pixelSize: 14
                    wrapMode: Text.WordWrap

                    anchors
                    {
                        left: parent.left
                        right: parent.right
                        leftMargin: -3
                    }
                }
            }

            CenteredField
            {
                visible: allowInsecureCheckBox.checked
                    && allowInsecureCheckBox.visible

                InsecureWarning
                {
                    width: parent.width
                }
            }

            CenteredField
            {
                visible: control.userType == UserSettingsGlobal.CloudUser

                Text
                {
                    width: parent.width
                    wrapMode: Text.WordWrap
                    font: Qt.font({pixelSize: 14, weight: Font.Normal})
                    color: ColorTheme.colors.light16
                    text: qsTr("You need to specify only user's email address.")
                        + "\n\n"
                        + qsTr("If user already has an account, he will see this system and will"
                        + " be able to log in to it. If not, we'll send an invitation to this"
                        + " address and user will see this system right after he creates an"
                        + " account.")
                }
            }

            Spacer {}

            CenteredField
            {
                text: qsTr("Permission Groups")

                GroupsComboBox
                {
                    id: groupsComboBox

                    width: parent.width
                }
            }
        }
    }
}
