// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15
import QtQuick.Layouts 1.15

import Nx 1.0

import nx.vms.client.desktop 1.0

import "../Components"

MembershipSettings
{
    id: control

    property var model

    showDescription: true
    editableProperty: "isMember"
    editableSection.property: "memberSection"
    editableSection.delegate: SectionHeader
    {
        width: parent.width - 16

        text: section === UserSettingsGlobal.kUsersSection
            ? qsTr("Users")
            : qsTr("Groups")
    }

    editableModel: AllowedMembersModel
    {
        sourceModel: control.model
    }

    summaryPlaceholder: [
        "image://svg/skin/user_settings/no_members.svg",
        qsTr("No members"),
        qsTr("Use controls on the left to add members")
    ]

    summaryText: qsTr("Members summary")
    summarySection.property: "memberSection"
    summarySection.delegate: Item
    {
        height: 36

        Text
        {
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 4
            font: Qt.font({pixelSize: 14, weight: Font.Normal})
            color: ColorTheme.colors.light16
            text: section === UserSettingsGlobal.kUsersSection
                ? qsTr("Users")
                : qsTr("Groups")
        }
    }

    // For full tree uncomment
    //summaryModel: control.model
    summaryModel: MembersSummaryModel
    {
        sourceModel: control.model
    }

    summaryDelegate: MembershipTreeItem
    {
        id: groupTreeHeader

        enabled: control.enabled && !model.isLdap
        offset: model.offset
        iconSource: model.isUser
            ? "image://svg/skin/user_settings/user_local.svg"
            : "image://svg/skin/user_settings/group_custom.svg"

        GlobalToolTip.text: model.offset > 0 ? qsTr("Inherits membership in current group") : ""

        onRemoveClicked: model.isMember = false
    }
}
