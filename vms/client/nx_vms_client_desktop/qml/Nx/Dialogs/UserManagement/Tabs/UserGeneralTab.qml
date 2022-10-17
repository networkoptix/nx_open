// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import QtQuick.Shapes 1.15

import Nx 1.0
import Nx.Controls 1.0

import nx.vms.client.desktop 1.0

import ".."
import "../Components"

Item
{
    id: control

    property bool isSelf: false

    property bool deleteAvailable: true
    property bool auditAvailable: true

    property alias login: userLoginText.text
    property bool loginEditable: false
    property alias fullName: userFullNameTextField.text
    property bool fullNameEditable: true
    property alias email: userEmailTextField.text
    property bool emailEditable: true
    property string password: ""
    property bool passwordEditable: true

    property alias userEnabled: enabledUserSwitch.checked
    property bool userEnabledEditable: true

    property alias allowInsecure: allowInsecureCheckBox.checked
    property bool allowInsecureEditable: true

    property alias globalPermissions: accessSummaryModel.globalPermissions
    property alias sharedResources: accessSummaryModel.sharedResources

    property var groups: []

    property bool editable: false
    property int userType: UserSettingsGlobal.LocalUser

    property var self

    property var editingContext

    signal deleteRequested()
    signal auditTrailRequested()

    signal groupClicked(var id)
    signal moreGroupsClicked()

    function validate()
    {
        let result = true

        if (userLoginText.enabled)
            result = userLoginText.validate()

        if (userEmailTextField.visible)
            return userEmailTextField.validate() && result

        return result
    }

    Scrollable
    {
        anchors.fill: parent

        ColumnLayout
        {
            width: parent.width
            spacing: 0

            Rectangle
            {
                id: loginPanel

                color: ColorTheme.colors.dark8
                Layout.fillWidth: true
                Layout.preferredHeight: Math.max(
                    103,
                    enabledUserSwitch.y + enabledUserSwitch.height + 22)

                Image
                {
                    id: userTypeIcon
                    x: 24
                    y: 24
                    width: 64
                    height: 64
                    source:
                    {
                        switch (control.userType)
                        {
                            case UserSettingsGlobal.LocalUser:
                                return "image://svg/skin/user_settings/user_type_local.svg"
                            case UserSettingsGlobal.CloudUser:
                                return "image://svg/skin/user_settings/user_type_cloud.svg"
                            case UserSettingsGlobal.LdapUser:
                                return "image://svg/skin/user_settings/user_type_ldap.svg"
                        }
                    }
                    sourceSize: Qt.size(width, height)
                }

                EditableLabel
                {
                    id: userLoginText

                    enabled: control.loginEditable

                    anchors.left: userTypeIcon.right
                    anchors.leftMargin: 24
                    anchors.top: userTypeIcon.top

                    anchors.right: parent.right
                    anchors.rightMargin: 16

                    validateFunc: control.self ? control.self.validateLogin : null
                }

                UserEnabledSwitch
                {
                    id: enabledUserSwitch

                    enabled: control.userEnabledEditable

                    anchors.top: userLoginText.bottom
                    anchors.topMargin: 14
                    anchors.left: userLoginText.left
                }

                Row
                {
                    anchors.top: userLoginText.bottom
                    anchors.topMargin: 14
                    anchors.right: parent.right
                    anchors.rightMargin: 20
                    spacing: 12

                    TextButton
                    {
                        id: auditTrailButton
                        icon.source: "image://svg/skin/user_settings/audit_trail.svg"
                        icon.width: 12
                        icon.height: 14
                        spacing: 8
                        text: qsTr("Audit Trail")
                        visible: control.auditAvailable
                        onClicked: control.auditTrailRequested()
                    }

                    TextButton
                    {
                        id: deleteButton
                        visible: control.deleteAvailable
                        icon.source: "image://svg/skin/user_settings/user_delete.svg"
                        icon.width: 12
                        icon.height: 14
                        spacing: 8
                        text: qsTr("Delete")
                        onClicked: control.deleteRequested()
                    }
                }
            }

            Rectangle
            {
                color: ColorTheme.colors.dark6

                Layout.fillWidth: true
                height: 1
            }

            Rectangle
            {
                color: ColorTheme.colors.dark7

                Layout.fillWidth: true
                Layout.fillHeight: true

                Column
                {
                    spacing: 8
                    anchors.left: parent.left
                    anchors.leftMargin: 16
                    anchors.right: parent.right
                    anchors.rightMargin: 16

                    SectionHeader
                    {
                        text: qsTr("Info")
                    }

                    CenteredField
                    {
                        text: qsTr("Full Name")

                        TextField
                        {
                            id: userFullNameTextField
                            width: parent.width
                            enabled: control.fullNameEditable
                                && control.userType != UserSettingsGlobal.CloudUser
                        }
                    }

                    CenteredField
                    {
                        visible: control.userType == UserSettingsGlobal.CloudUser && control.isSelf

                        Item
                        {
                            id: externalLink

                            width: childrenRect.width
                            height: childrenRect.height
                            property string link: "?"

                            Text
                            {
                                id: linkText
                                text: qsTr("Account Settings")
                                color: ColorTheme.colors.brand_core
                                font: Qt.font({pixelSize: 14, weight: Font.Normal, underline: true})
                            }

                            Shape
                            {
                                id: linkArrow

                                width: 5
                                height: 5

                                anchors.left: linkText.right
                                anchors.verticalCenter: linkText.verticalCenter
                                anchors.verticalCenterOffset: -1
                                anchors.leftMargin: 6

                                ShapePath
                                {
                                    strokeWidth: 1
                                    strokeColor: linkText.color
                                    fillColor: "transparent"

                                    startX: 1; startY: 0
                                    PathLine { x: linkArrow.width; y: 0 }
                                    PathLine { x: linkArrow.width; y: linkArrow.height - 1 }
                                    PathMove { x: linkArrow.width; y: 0 }
                                    PathLine { x: 0; y: linkArrow.height }
                                }
                            }

                            MouseArea
                            {
                                id: mouseArea
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked:
                                {
                                    Qt.openUrlExternally(UserSettingsGlobal.accountManagementUrl())
                                }
                            }
                        }
                    }

                    CenteredField
                    {
                        text: qsTr("Email")
                        visible: control.userType != UserSettingsGlobal.CloudUser

                        TextFieldWithValidator
                        {
                            id: userEmailTextField
                            width: parent.width
                            enabled: control.emailEditable
                            validateFunc: (text) =>
                            {
                                return control.self && enabled
                                    ? control.self.validateEmail(
                                        text, control.userType == UserSettingsGlobal.CloudUser)
                                    : ""
                            }
                        }
                    }

                    CenteredField
                    {
                        // Change password.

                        visible: control.passwordEditable
                            && control.userType == UserSettingsGlobal.LocalUser

                        Item
                        {
                            height: changePasswordButton.height + 8

                            Component
                            {
                                id: changePasswordDialog

                                PasswordChangeDialog
                                {
                                    transientParent: control.Window.window
                                    visible: false

                                    login: control.login
                                    askCurrentPassword: control.isSelf
                                    currentPasswordValidator: control.isSelf
                                        ? dialog.self.validateCurrentPassword
                                        : null

                                    onAccepted:
                                    {
                                        control.password = newPassword
                                    }
                                }
                            }

                            Button
                            {
                                y: 4
                                id: changePasswordButton
                                text: qsTr("Change password")

                                onClicked:
                                {
                                    changePasswordDialog.createObject(control).openNew()
                                }
                            }
                        }
                    }

                    CenteredField
                    {
                        // Allow digest authentication.

                        visible: control.userType != UserSettingsGlobal.CloudUser

                        Component
                        {
                            id: changePasswordDigestDialog

                            PasswordChangeDialog
                            {
                                transientParent: control.Window.window
                                visible: false

                                text: qsTr("Set password to enable insecure authentication")
                                login: control.login
                                askCurrentPassword: control.isSelf
                                showLogin: true

                                currentPasswordValidator: control.isSelf
                                    ? dialog.self.validateCurrentPassword
                                    : null

                                onAccepted:
                                {
                                    allowInsecureCheckBox.checked = true
                                    control.password = newPassword
                                }
                            }
                        }

                        CheckBox
                        {
                            id: allowInsecureCheckBox
                            text: qsTr("Allow insecure (digest) authentication")
                            enabled: allowInsecureEditable

                            nextCheckState: () =>
                            {
                                if (checkState === Qt.Unchecked)
                                    changePasswordDigestDialog.createObject(control).openNew()

                                return Qt.Unchecked
                            }
                        }
                    }

                    CenteredField
                    {
                        visible: allowInsecureCheckBox.checked

                        InsecureWarning
                        {
                            width: parent.width
                        }
                    }

                    SectionHeader
                    {
                        text: qsTr("Groups")
                    }

                    GroupsFlow
                    {
                        id: groupsFlow

                        width: parent.width

                        rowLimit: 2
                        items: control.groups

                        onGroupClicked: (id) => { control.groupClicked(id) }
                        onMoreClicked: control.moreGroupsClicked()
                    }

                    Text
                    {
                        visible: control.groups.length === 0
                        font: Qt.font({pixelSize: 14, weight: Font.Normal})
                        color: ColorTheme.colors.light16
                        text: qsTr("Not a member of any group")
                    }

                    SectionHeader
                    {
                        text: qsTr("Custom Permissions")
                    }

                    PermissionSummary
                    {
                        Layout.topMargin: 8

                        model: CustomAccessSummaryModel
                        {
                            id: accessSummaryModel
                        }
                    }
                }
            }
        }
    }

    Informer
    {
        id: informer

        visible: false

        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: 78
        color: ColorTheme.colors.red_d4

        text: qsTr("This user is not found in LDAP database and is not able to log in.")
        buttonText: qsTr("Delete")
        buttonIcon.width: 12
        buttonIcon.height: 14
        buttonIcon.source: "image://svg/skin/user_settings/user_delete.svg"

        onClicked: informer.visible = false
    }
}
