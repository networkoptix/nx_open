// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15
import QtQuick.Layouts 1.15

import Nx 1.0
import Nx.Controls 1.0

import nx.vms.client.desktop 1.0

import "../Components"

MembershipSettings
{
    id: control

    property var model

    signal addGroupClicked()

    component AddGroupButton: TextButton
    {
        anchors.right: parent.right
        anchors.rightMargin: 2
        anchors.top: parent.top
        anchors.topMargin: parent.baselineOffset - baselineOffset - 1

        text: '+ ' + qsTr("Add Group")

        font: Qt.font({pixelSize: 14, weight: Font.Normal})
        color: ColorTheme.colors.light16

        onClicked: control.addGroupClicked()
    }

    editableProperty: "isParent"
    editableSection.property: "groupSection"
    editableSection.delegate: SectionHeader
    {
        width: parent.width - 16

        text: section === UserSettingsGlobal.kBuiltInGroupsSection
            ? qsTr("Built In")
            : qsTr("Custom")

        AddGroupButton
        {
            visible: section === UserSettingsGlobal.kCustomGroupsSection
        }
    }

    editableFooter: SectionHeader
    {
        id: customHeader

        visible: !currentSearchRegExp && model.customGroupCount === 0

        width: parent.width - 16
        text: qsTr("Custom")

        AddGroupButton
        {
        }

        Text
        {
            anchors.top: customHeader.bottom
            anchors.topMargin: 2
            anchors.left: customHeader.left
            anchors.leftMargin: 14

            font: Qt.font({pixelSize: 12, weight: Font.Normal})
            color: ColorTheme.colors.light16

            text: qsTr("No custom groups yet")
        }
    }

    editableModel: AllowedParentsModel
    {
        sourceModel: control.model
    }

    editablePlaceholder: Item
    {
        anchors.centerIn: parent
        width: 200
        height: parent.height

        Placeholder
        {
            anchors.verticalCenterOffset: -32
            imageSource: "image://svg/skin/user_settings/no_results.svg"
            text: qsTr("No groups found")
            additionalText: qsTr("Change search criteria or create a new group")

            Button
            {
                Layout.alignment: Qt.AlignHCenter

                text: qsTr("Add Group")

                iconUrl: "image://svg/skin/user_settings/add.svg"
                iconWidth: 20
                iconHeight: 20
                leftPadding: 12
                rightPadding: 16
                iconSpacing: 2

                onClicked: control.addGroupClicked()
            }
        }
    }

    summaryText: qsTr("Selected groups")
    summaryModel: ParentGroupsModel
    {
        sourceModel: ParentGroupsProvider
        {
            context: control.model.editingContext
        }
    }

    summaryPlaceholder: [
        "image://svg/skin/user_settings/no_groups.svg",
        qsTr("No groups"),
        qsTr("Use controls on the left to add to a group")
    ]

    summaryDelegate: Item
    {
        id: groupTree
        width: parent ? parent.width : 0
        height: groupTreeHeader.height

        property var groupId: model.id

        MembershipTreeItem
        {
            id: groupTreeHeader
            onRemoveClicked: model.isParent = false
            iconSource: model.isPredefined
                ? "image://svg/skin/user_settings/group_built_in.svg"
                : model.isLdap
                    ? "image://svg/skin/user_settings/group_ldap.svg"
                    : "image://svg/skin/user_settings/group_custom.svg"

            offset: 0
            interactive: model.isParent
            enabled: control.enabled && !model.isLdap && model.isParent
            GlobalToolTip.text: model.isLdap
                ? qsTr("LDAP group membership is managed in LDAP")
                : ""
        }
    }
}
