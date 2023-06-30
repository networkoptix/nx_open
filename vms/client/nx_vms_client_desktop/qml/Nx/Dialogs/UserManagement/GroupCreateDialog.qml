// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.11
import QtQuick.Controls 2.15
import QtQuick.Window 2.15
import QtQuick.Layouts 1.15

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
    objectName: "groupCreateDialog" //< For autotesting.

    modality: Qt.ApplicationModal

    width: 0
    height: 0
    minimumWidth: Math.max(768, tabControl.implicitWidth)
    minimumHeight: 672

    color: ColorTheme.colors.dark7

    // State properties.
    property alias groupId: membersModel.groupId
    property alias name: generalSettings.name
    property bool nameEditable //< Not used by current dialog type.
    property bool isLdap: false
    property bool isPredefined: false
    property alias description: generalSettings.description
    property bool descriptionEditable //< Not used by current dialog type.
    property alias parentGroups: membersModel.parentGroups
    property alias groups: membersModel.groups
    property alias users: membersModel.users
    property alias globalPermissions: globalPermissionsModel.globalPermissions
    property alias sharedResources: membersModel.sharedResources

    property bool parentGroupsEditable //< Not used by current dialog type.
    property bool membersEditable //< Not used by current dialog type.
    property bool permissionsEditable //< Not used by current dialog type.
    property bool nameIsUnique //< Not used by current dialog type.

    // Mapped to dialog property.
    property alias tabIndex: tabControl.currentTabIndex
    property bool isSaving: false
    property alias editingContext: membersModel.editingContext
    property var self
    property int continuousSync //< Not used by current dialog type.
    property bool deleteAvailable //< Not used by current dialog type.
    property bool ldapError: false //< Not used by current dialog type.

    title: qsTr("New Group")

    validateFunc: () =>
    {
        if (generalSettings.validate())
            return true

        dialog.tabIndex = 0
        return false
    }

    MembersModel
    {
        id: membersModel
    }

    DialogTabControl
    {
        id: tabControl

        anchors.fill: parent
        anchors.bottomMargin: buttonBox.height

        dialogLeftPadding: dialog.leftPadding
        dialogRightPadding: dialog.rightPadding
        tabBar.spacing: 0

        Tab
        {
            button: DialogTabButton
            {
                text: qsTr("General")
            }

            page: GroupCreateTab
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

                infoText: dialog.self ? dialog.self.kInfoText : ""

                model: membersModel
                allowAddGroup: false

                enabled: !dialog.isSaving
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

        Tab
        {
            button: DialogTabButton
            {
                text: qsTr("Members")
            }

            page: GroupMembersTab
            {
                id: membersSettings
                anchors.fill: parent

                enabled: !dialog.isSaving

                model: membersModel
            }
        }
    }

    buttonBox: DialogButtonBox
    {
        id: buttonBox
        buttonLayout: DialogButtonBox.KdeLayout

        standardButtons: DialogButtonBox.Cancel

        Button
        {
            text: dialog.isSaving ? "" : qsTr("Add Group")
            width: Math.max(implicitWidth, 80)
            isAccentButton: true
            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole

            NxDotPreloader
            {
                anchors.centerIn: parent
                running:  dialog.isSaving
            }
        }
    }
}
