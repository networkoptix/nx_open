// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import QtQuick.Shapes 1.15

import Nx 1.0
import Nx.Core 1.0
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

    property alias model: groupsComboBox.model
    property alias parentGroupsEditable: groupsComboBox.enabled

    property bool enabled: true
    property int userType: UserSettingsGlobal.LocalUser

    property bool linkEditable: true
    property alias linkValidFrom: linkDates.linkValidFrom
    property alias linkValidUntil: linkDates.linkValidUntil
    property alias expiresAfterLoginS: linkDates.expiresAfterLoginS
    property alias revokeAccessEnabled: linkDates.revokeAccessEnabled
    property alias firstLoginTime: linkDates.firstLoginTime

    property alias displayOffsetMs: linkDates.displayOffsetMs

    property bool ldapError: false
    property bool continuousSync: true

    property var self

    property var editingContext

    signal deleteRequested()
    signal auditTrailRequested()

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

    ColumnLayout
    {
        anchors.fill: parent

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
                        case UserSettingsGlobal.TemporaryUser:
                            return "image://svg/skin/user_settings/user_type_local_temp.svg"
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

                enabled: control.loginEditable && control.enabled

                anchors.left: userTypeIcon.right
                anchors.leftMargin: 24
                anchors.top: userTypeIcon.top

                anchors.right: parent.right
                anchors.rightMargin: 16

                validateFunc: control.self
                    ? (text) => (control.userType == UserSettingsGlobal.CloudUser
                        ? control.self.validateEmail(text) : control.self.validateLogin(text))
                    : null

                Connections
                {
                    target: control.model

                    function onUserIdChanged() { userLoginText.forceFinishEdit() }
                }
            }

            UserEnabledSwitch
            {
                id: enabledUserSwitch

                enabled: control.userEnabledEditable && control.enabled

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
                    enabled: control.enabled
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

        Scrollable
        {
            id: scroll

            Layout.fillWidth: true
            Layout.fillHeight: true

            contentItem: Rectangle
            {
                color: ColorTheme.colors.dark7

                width: scroll.width
                height: childrenRect.height

                Column
                {
                    id: contentColumn

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
                            readOnly: !control.fullNameEditable
                                || !control.enabled
                                || control.userType == UserSettingsGlobal.CloudUser
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
                            readOnly: !(control.emailEditable && control.enabled)

                            fixupFunc:
                                (text) => (control.self ? control.self.extractEmail(text) : text)

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
                                    id: dialog

                                    transientParent: control.Window.window
                                    visible: false

                                    login: control.login
                                    askCurrentPassword: control.isSelf
                                    currentPasswordValidator: control.isSelf
                                        ? (text) => control.self.validateCurrentPassword(text)
                                        : null

                                    onAccepted:
                                    {
                                        control.password = newPassword
                                    }

                                    Connections
                                    {
                                        target: control.Window.window
                                        function onClosing() { dialog.reject() }
                                    }
                                }
                            }

                            Button
                            {
                                y: 4
                                id: changePasswordButton
                                text: qsTr("Change password")
                                enabled: control.enabled

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
                            && control.userType != UserSettingsGlobal.TemporaryUser

                        Component
                        {
                            id: changePasswordDigestDialog

                            PasswordChangeDialog
                            {
                                id: dialog

                                transientParent: control.Window.window
                                visible: false

                                text: qsTr("Set password to enable insecure authentication")
                                login: control.login
                                askCurrentPassword: control.isSelf
                                showLogin: true

                                currentPasswordValidator: control.isSelf
                                    ? (text) => control.self.validateCurrentPassword(text)
                                    : null

                                onAccepted:
                                {
                                    allowInsecureCheckBox.checked = true
                                    control.password = newPassword
                                }

                                Connections
                                {
                                    target: control.Window.window
                                    function onClosing() { dialog.reject() }
                                }
                            }
                        }

                        CheckBox
                        {
                            id: allowInsecureCheckBox

                            text: qsTr("Allow insecure (digest) authentication")
                            font.pixelSize: 14

                            enabled: control.allowInsecureEditable && control.enabled

                            wrapMode: Text.WordWrap

                            anchors
                            {
                                left: parent.left
                                right: parent.right
                                leftMargin: -3
                            }

                            nextCheckState: () =>
                            {
                                // Enabling digest for LDAP user does not require password reset.
                                if (control.userType == UserSettingsGlobal.LdapUser)
                                    return checkState === Qt.Unchecked ? Qt.Checked : Qt.Unchecked

                                if (checkState === Qt.Unchecked)
                                    changePasswordDigestDialog.createObject(control).openNew()

                                return Qt.Unchecked
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

                    Item
                    {
                        height: 64 - contentColumn.spacing * 2
                        width: parent.width

                        visible: control.userType == UserSettingsGlobal.TemporaryUser
                                && !control.linkEditable
                                && linkDates.expiresInMs > 0

                        CenteredField
                        {
                            id: expiresTextField

                            y: contentColumn.spacing

                            text: qsTr("Access expires")

                            Text
                            {
                                anchors.baseline: parent.baseline

                                width: parent.width

                                text: (control.self || "") && "%1%2"
                                    .arg(linkDates.expirationDateText)
                                    .arg(control.self.durationFormat(linkDates.expiresInMs))

                                color: ColorTheme.colors.light4
                                font: Qt.font({pixelSize: 14, weight: Font.Normal})
                            }
                        }

                    }

                    CenteredField
                    {
                        text: qsTr("Permission Groups")

                        GroupsComboBox
                        {
                            id: groupsComboBox

                            width: parent.width
                        }
                    }

                    SectionHeader
                    {
                        id: accessLinkSection

                        text: qsTr("Access Link")
                        visible: control.userType == UserSettingsGlobal.TemporaryUser
                            && control.linkEditable
                    }

                    ColumnLayout
                    {
                        id: accessLinkLayout

                        visible: accessLinkSection.visible

                        width: parent.width
                        spacing: 16

                        function openNewLinkDialog(showWarning)
                        {
                            const newLinkDialog = newLinkDialogComponent.createObject(control)

                            newLinkDialog.linkValidUntil = self.newValidUntilDate()
                            newLinkDialog.showWarning = false

                            newLinkDialog.openNew()
                        }

                        function openResetLinkDialog()
                        {
                            const newLinkDialog = newLinkDialogComponent.createObject(control)

                            newLinkDialog.linkValidUntil = control.linkValidUntil
                            if (control.revokeAccessEnabled)
                            {
                                newLinkDialog.revokeAccessEnabled = true
                                newLinkDialog.expiresAfterLoginS = control.expiresAfterLoginS
                            }

                            newLinkDialog.showWarning = true
                            newLinkDialog.openNew()
                        }

                        Component
                        {
                            id: newLinkDialogComponent

                            NewLinkDialog
                            {
                                login: control.login
                                displayOffsetMs: control.displayOffsetMs
                                isSaving: !control.enabled
                                transientParent: control.Window.window

                                SessionAware.onTryClose: reject()

                                onAccepted:
                                {
                                    control.self.onResetLink(
                                        linkValidUntil,
                                        revokeAccessEnabled
                                            ? expiresAfterLoginS
                                            : -1,
                                        ok => { if (ok) close() })
                                }

                                onRejected:
                                {
                                    if (isSaving)
                                        control.self.cancelRequest()
                                }
                            }
                        }

                        TemporaryLinkDates
                        {
                            id: linkDates

                            property var currentServerTimePointMs:
                                NxGlobals.syncTimeCurrentTimePointMs()
                            readonly property bool expired: expiresInMs <= 0
                            readonly property var expiresInMs:
                                expirationDate.getTime() - currentServerTimePointMs

                            Timer
                            {
                                interval: 1000
                                running: control.userType == UserSettingsGlobal.TemporaryUser
                                repeat: true
                                onTriggered:
                                {
                                    linkDates.currentServerTimePointMs =
                                        NxGlobals.syncTimeCurrentTimePointMs()
                                }
                            }
                        }

                        Text
                        {
                            visible: !linkDates.expired

                            wrapMode: Text.WordWrap
                            textFormat: Text.StyledText

                            color: ColorTheme.colors.light16
                            font: Qt.font({pixelSize: 14, weight: Font.Normal})
                            width: parent.width

                            text: linkDates.validityDatesText
                        }

                        RowLayout
                        {
                            visible: !linkDates.expired

                            spacing: 8

                            Button
                            {
                                id: copyLinkButton

                                property bool copied: false
                                readonly property string copyLinkText: qsTr("Copy Link")
                                readonly property string copiedText: qsTr("Copied",
                                    "Copied here means that a link is copied")
                                enabled: control.enabled
                                text: copied ? copiedText : copyLinkText
                                icon.source:
                                    copied ? "image://svg/skin/user_settings/copied.svg" : ""
                                icon.width: 20
                                icon.height: 20
                                Layout.preferredWidth:
                                {
                                    const copyLinkWidth = leftPaddingOnlyText
                                        + fontMetrics.advanceWidth(copyLinkText) + rightPadding
                                    const copiedWidth = leftPaddingWithIcon + icon.width + 4
                                        + fontMetrics.advanceWidth(copiedText) + rightPadding
                                    return Math.max(copyLinkWidth, copiedWidth)
                                }

                                FontMetrics
                                {
                                    id: fontMetrics
                                    font: copyLinkButton.font
                                }

                                Timer
                                {
                                    interval: 3000
                                    repeat: false
                                    running: copyLinkButton.copied
                                    onTriggered: copyLinkButton.copied = false
                                }

                                onClicked:
                                {
                                    control.self.onCopyLink()
                                    copied = true
                                }
                            }

                            TextButton
                            {
                                icon.source: "image://svg/skin/user_settings/terminate_link.svg"
                                icon.width: 20
                                icon.height: 20
                                spacing: 4
                                text: qsTr("Terminate")
                                enabled: control.enabled

                                onClicked: control.self.onTerminateLink()
                            }

                            TextButton
                            {
                                icon.source: "image://svg/skin/user_settings/reset_link.svg"
                                icon.width: 20
                                icon.height: 20
                                spacing: 4
                                text: qsTr("New Link...")
                                enabled: control.enabled

                                onClicked: accessLinkLayout.openResetLinkDialog()
                            }
                        }

                        Text
                        {
                            Layout.alignment: Qt.AlignHCenter
                            visible: linkDates.expired

                            text: qsTr("No valid link for this user")
                            color: ColorTheme.colors.light16
                            font: Qt.font({pixelSize: 14, weight: Font.Normal})
                        }

                        Button
                        {
                            Layout.alignment: Qt.AlignHCenter
                            visible: linkDates.expired
                            enabled: control.enabled

                            text: qsTr("New Link...")
                            onClicked: accessLinkLayout.openNewLinkDialog()
                        }
                    }
                }
            }
        }
    }

    DialogBanner
    {
        id: bannerUserNotFound

        style: DialogBanner.Style.Error

        visible: false
        closeVisible: true

        Binding
        {
            target: bannerUserNotFound
            property: "visible"
            value: control.ldapError
        }

        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right

        text: qsTr("This user is not found in LDAP database and is not able to log in.")
        buttonText: control.deleteAvailable && control.enabled ? qsTr("Delete") : ""
        buttonIcon: "image://svg/skin/user_settings/trash.svg"

        onButtonClicked: control.deleteRequested()
        onCloseClicked: bannerUserNotFound.visible = false
    }

    DialogBanner
    {
        id: bannerLdapContinousSyncDisabled

        style: DialogBanner.Style.Warning
        visible: control.userType === UserSettingsGlobal.LdapUser && !control.continuousSync
        closeVisible: true

        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right

        text: qsTr("When Continuous Sync is disabled, user’s membership in groups do not "
            + "synchronize automatically. To update this information, initiate a manual sync.")
        onCloseClicked: bannerLdapContinousSyncDisabled.visible = false
    }
}
