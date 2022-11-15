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

    editableProperty: "isParent"
    editableSection.property: "groupSection"
    editableSection.delegate: SectionHeader
    {
        width: parent.width - 16

        text: section === UserSettingsGlobal.kBuiltInGroupsSection
            ? qsTr("Built In")
            : qsTr("Custom")

        TextButton
        {
            visible: section === UserSettingsGlobal.kCustomGroupsSection

            anchors.right: parent.right
            anchors.rightMargin: 2
            anchors.top: parent.top
            anchors.topMargin: parent.baselineOffset - baselineOffset - 1

            text: '+ ' + qsTr("Add")

            font: Qt.font({pixelSize: 14, weight: Font.Normal})
            color: ColorTheme.colors.light16

            onClicked: control.addGroupClicked()
        }
    }

    editableModel: AllowedParentsModel
    {
        sourceModel: control.model
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
