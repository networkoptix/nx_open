// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

import Nx.Core
import Nx.Controls
import Nx.Models

import nx.vms.client.core
import nx.vms.client.desktop

import ".."
import "Tabs"
import "Components"

DialogWithState
{
    id: dialog
    objectName: "userEditDialog" //< For autotesting.

    modality: Qt.NonModal

    width: 0
    height: 0
    minimumWidth: Math.max(768, tabControl.implicitWidth)
    minimumHeight: 672

    color: ColorTheme.colors.dark7

    // State properties.
    property alias userType: generalSettings.userType
    property alias isSelf: generalSettings.isSelf
    property alias isOrgUser: generalSettings.isOrgUser
    property alias userId: membersModel.userId
    property alias deleteAvailable: generalSettings.deleteAvailable
    property alias auditAvailable: generalSettings.auditAvailable
    property alias login: generalSettings.login
    property alias loginEditable: generalSettings.loginEditable
    property alias fullName: generalSettings.fullName
    property alias fullNameEditable: generalSettings.fullNameEditable
    property alias email: generalSettings.email
    property alias emailEditable: generalSettings.emailEditable
    property alias password: generalSettings.password
    property alias passwordEditable: generalSettings.passwordEditable
    property alias locale: generalSettings.locale
    property alias localeEditable: generalSettings.localeEditable
    property alias userEnabled: generalSettings.userEnabled
    property alias userEnabledEditable: generalSettings.userEnabledEditable
    property alias allowInsecure: generalSettings.allowInsecure
    property alias allowInsecureEditable: generalSettings.allowInsecureEditable
    property alias insecureAuthEnabledBySiteSettings: generalSettings.insecureAuthEnabledBySiteSettings
    property alias parentGroups: membersModel.parentGroups
    property bool parentGroupsEditable: true
    property alias globalPermissions: globalPermissionsModel.globalPermissions
    property alias sharedResources: membersModel.sharedResources
    property bool permissionsEditable: true
    property bool userIsNotRegisteredInCloud: false

    property alias linkEditable: generalSettings.linkEditable
    property alias linkValidFrom: generalSettings.linkValidFrom
    property alias linkValidUntil: generalSettings.linkValidUntil
    property alias expiresAfterLoginS: generalSettings.expiresAfterLoginS
    property alias revokeAccessEnabled: generalSettings.revokeAccessEnabled
    property alias displayOffsetMs: generalSettings.displayOffsetMs
    property alias firstLoginTime: generalSettings.firstLoginTime
    property alias nameIsUnique: generalSettings.nameIsUnique

    // Mapped to dialog property.
    property alias tabIndex: tabControl.currentTabIndex
    property bool isSaving: false
    property bool ldapError: false
    property bool ldapOffline: true
    property alias linkReady: generalSettings.linkReady
    property int continuousSync: LdapSettings.Sync.usersAndGroups
    property alias editingContext: membersModel.editingContext
    property var self

    signal deleteRequested()
    signal auditTrailRequested()
    signal addGroupRequested()

    title: qsTr("User - %1").arg(login)

    ContextHelp.topicId: HelpTopic.UserRoles

    MembersModel
    {
        id: membersModel
    }

    DialogTabControl
    {
        id: tabControl

        anchors.fill: parent
        anchors.bottomMargin: buttonBox.height + banners.height

        dialogLeftPadding: dialog.leftPadding
        dialogRightPadding: dialog.rightPadding
        tabBar.spacing: 0

        Tab
        {
            button: PrimaryTabButton
            {
                text: qsTr("General")
            }

            page: UserGeneralTab
            {
                id: generalSettings
                anchors.fill: parent

                self: dialog.self
                userId: dialog.userId

                model: membersModel
                parentGroupsEditable: dialog.parentGroupsEditable
                enabled: !dialog.isSaving
                ldapError: dialog.ldapError
                ldapOffline: dialog.ldapOffline
                continuousSync: dialog.continuousSync

                onDeleteRequested: dialog.deleteRequested()
                onAuditTrailRequested: dialog.auditTrailRequested()
                onMoreGroupsClicked: dialog.tabIndex = 1
                editingContext: membersModel.editingContext
            }
        }

        Tab
        {
            button: PrimaryTabButton
            {
                text: qsTr("Groups")
            }

            page: ParentGroupsTab
            {
                id: groupsSettings
                anchors.fill: parent

                model: membersModel
                enabled: !dialog.isSaving
                editable: dialog.parentGroupsEditable

                onAddGroupRequested: dialog.addGroupRequested()
            }
        }

        Tab
        {
            button: PrimaryTabButton
            {
                text: qsTr("Resources")
            }

            page: PermissionsTab
            {
                id: permissionSettings
                anchors.fill: parent

                editingEnabled: dialog.permissionsEditable
                enabled: !dialog.isSaving

                buttonBox: buttonBox
                editingContext: membersModel.editingContext
            }
        }

        Tab
        {
            button: PrimaryTabButton
            {
                text: qsTr("Global Permissions")
            }

            page: GlobalPermissionsTab
            {
                id: globalPermissionSettings
                anchors.fill: parent

                editable: dialog.permissionsEditable && !dialog.isSaving

                model: GlobalPermissionsModel
                {
                    id: globalPermissionsModel
                    context: membersModel.editingContext
                }
            }
        }

        onTabSwitched:
        {
            if (tabIndex === 0)
                validateFunc()
        }
    }

    Column
    {
        id: banners

        anchors.bottom: buttonBox.top
        anchors.left: parent.left
        anchors.right: parent.right
        spacing: 2
        z: -1

        DialogBanner
        {
            style: DialogBanner.Style.Warning
            closeable: true
            watchToReopen: dialog.userId
            width: banners.width
            visible: dialog.userIsNotRegisteredInCloud && !closed

            text: qsTr("This user has not yet signed up for %1",
                "%1 is the cloud name").arg(Branding.cloudName())
        }

        DialogBanner
        {
            id: temporaryUserWarning

            style: DialogBanner.Style.Info
            width: banners.width

            closeable: true
            watchToReopen: dialog.userId
            visible: !!text && !dialog.isSelf && !closed

            text: dialog.userType === UserSettingsGlobal.TemporaryUser
                ? dialog.self.warningForTemporaryUser(
                    parentGroups,
                    sharedResources,
                    globalPermissions)
                : ""
        }
    }

    validateFunc: () =>
    {
        if (generalSettings.validate())
            return true

        tabControl.switchTab(0, () => findAndFocusTextWithWarning(generalSettings))
        return false
    }

    buttonBox: DialogButtonBoxWithPreloader
    {
        id: buttonBox

        modified: dialog.modified
        preloaderButton: dialog.isSaving
            ? (dialog.self.isOkClicked()
                ? DialogButtonBox.Ok
                : DialogButtonBox.Apply)
            : undefined
    }
}
