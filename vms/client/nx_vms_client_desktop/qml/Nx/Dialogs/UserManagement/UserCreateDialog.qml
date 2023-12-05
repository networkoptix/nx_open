// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15
import QtQuick.Layouts 1.15

import Nx 1.0
import Nx.Core 1.0
import Nx.Controls 1.0
import Nx.Models 1.0

import nx.client.desktop 1.0
import nx.vms.client.core 1.0
import nx.vms.client.desktop 1.0

import ".."
import "Tabs"

DialogWithState
{
    id: dialog
    objectName: "userCreateDialog" //< For autotesting.

    modality: Qt.ApplicationModal

    width: 0
    height: 0
    minimumWidth: Math.max(768, tabControl.implicitWidth)
    minimumHeight: 700

    color: ColorTheme.colors.dark7

    // State properties.
    property alias userType: generalSettings.userType
    property bool isSelf: false //< Not used by current dialog type.
    property alias userId: membersModel.userId
    property bool deleteAvailable: false
    property bool auditAvailable: false
    property alias login: generalSettings.login
    property bool loginEditable: true
    property alias fullName: generalSettings.fullName
    property bool fullNameEditable: true
    property alias email: generalSettings.email
    property bool emailEditable: true
    property string password: generalSettings.password
    property bool passwordEditable: true
    property alias userEnabled: generalSettings.userEnabled
    property bool userEnabledEditable: true
    property alias allowInsecure: generalSettings.allowInsecure
    property bool allowInsecureEditable: true
    property alias parentGroups: membersModel.parentGroups
    property bool parentGroupsEditable: true
    property alias globalPermissions: globalPermissionsModel.globalPermissions
    property alias sharedResources: membersModel.sharedResources
    property bool permissionsEditable: true  //< Not used by current dialog type.
    property bool userIsNotRegisteredInCloud: false //< Not used by current dialog type.

    property bool linkEditable: true
    property date linkValidFrom
    property alias linkValidUntil: generalSettings.linkValidUntil
    property alias expiresAfterLoginS: generalSettings.expiresAfterLoginS
    property alias revokeAccessEnabled: generalSettings.revokeAccessEnabled
    property int displayOffsetMs  //< Not used by current dialog type.
    property bool nameIsUnique //< Not used by current dialog type.

    // Mapped to dialog property.
    property alias tabIndex: tabControl.currentTabIndex
    property bool isSaving: false
    property bool ldapError: false
    property bool ldapOffline: true
    property bool linkReady: true
    property int continuousSync //< Not used by current dialog type.
    property alias editingContext: membersModel.editingContext
    property var self
    property date firstLoginTime //< Not used by current dialog type.

    signal addGroupRequested()

    title: qsTr("New User")

    validateFunc: () =>
    {
        if (generalSettings.validate())
            return true

        tabControl.switchTab(0, () => findAndFocusTextWithWarning(generalSettings))
        return false
    }

    MembersModel
    {
        id: membersModel
        temporary: userType == UserSettingsGlobal.TemporaryUser
    }

    DialogTabControl
    {
        id: tabControl

        anchors.fill: parent
        anchors.bottomMargin: buttonBox.height + (banner.visible ? banner.height : 0)

        dialogLeftPadding: dialog.leftPadding
        dialogRightPadding: dialog.rightPadding
        tabBar.spacing: 0

        Tab
        {
            button: DialogTabButton
            {
                text: qsTr("General")
            }

            page: UserCreateTab
            {
                id: generalSettings
                anchors.fill: parent
                self: dialog.self
                enabled: !dialog.isSaving
                model: membersModel
            }
        }
        Tab
        {
            button: DialogTabButton
            {
                text: qsTr("Groups")
            }

            page: ParentGroupsTab
            {
                id: groupsSettings
                anchors.fill: parent

                model: membersModel

                enabled: !dialog.isSaving

                onAddGroupRequested: dialog.addGroupRequested()
            }
        }
        Tab
        {
            button: DialogTabButton
            {
                text: qsTr("Resources")
            }

            page: PermissionsTab
            {
                id: permissionSettings
                anchors.fill: parent

                enabled: !dialog.isSaving

                buttonBox: buttonBox
                editingContext: membersModel.editingContext
            }
        }

        Tab
        {
            button: DialogTabButton
            {
                text: qsTr("Global Permissions")
            }

            page: GlobalPermissionsTab
            {
                id: globalPermissionSettings
                anchors.fill: parent

                enabled: !dialog.isSaving

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

    DialogBanner
    {
        id: banner

        style: DialogBanner.Style.Info
        closeable: true

        visible: !!text && !closed

        text: dialog.userType === UserSettingsGlobal.TemporaryUser
            ? dialog.self.warningForTemporaryUser(
                parentGroups,
                sharedResources,
                globalPermissions)
            : ""

        anchors.bottom: buttonBox.top
        anchors.left: parent.left
        anchors.right: parent.right
        z: -1
    }

    buttonBox: DialogButtonBox
    {
        id: buttonBox
        buttonLayout: DialogButtonBox.KdeLayout

        standardButtons: DialogButtonBox.Cancel

        Button
        {
            text: dialog.isSaving ? "" : qsTr("Add User")
            width: Math.max(implicitWidth, 80)
            isAccentButton: true
            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole

            NxDotPreloader
            {
                anchors.centerIn: parent
                running: dialog.isSaving
            }
        }
    }
}
