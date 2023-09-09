// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15
import QtQuick.Layouts 1.15

import Nx 1.0
import Nx.Core 1.0
import Nx.Controls 1.0

import nx.vms.client.desktop 1.0

import "../Components"

MembershipSettings
{
    id: control

    property var model

    showDescription: true
    editableProperty: "isMember"
    enabledProperty: "canEditParents"
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
        control.editable ? qsTr("Use controls on the left to add members") : ""
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

    summaryModel: MembersSummaryModel
    {
        sourceModel: control.model
    }

    component SummaryItem: MembershipTreeItem
    {
        enabled: control.enabled && model.canEditParents
        offset: model.offset
        iconSource: model.isUser
            ? model.isLdap
                ? "image://svg/skin/user_settings/user_ldap.svg"
                : (model.isTemporary
                    ? "image://svg/skin/user_settings/user_local_temp.svg"
                    : "image://svg/skin/user_settings/user_local.svg")
            : model.isLdap
                ? "image://svg/skin/user_settings/group_ldap.svg"
                : "image://svg/skin/user_settings/group_custom.svg"
        GlobalToolTip.text: model.offset > 0 ? qsTr("Inherits membership in current group") : ""
    }

    summaryDelegate: SummaryItem
    {
        id: groupTreeHeader

        onRemoveClicked: model.isMember = false

        height: subTree.visible ? 24 + subTree.height : 24
        property var currentView: ListView.view
        property var groupId: model.isUser ? NxGlobals.uuid("") : model.id

        Item
        {
            id: subTree

            visible: !model.isUser && !currentSearchRegExp

            width: parent.width
            height: 24 * itemCount
            y: 24

            property int itemCount: recursiveMembers.count

            property real viewTop: 0
            property real viewBottom: 0

            Connections
            {
                target: groupTreeHeader.currentView
                function onContentYChanged()
                {
                    subTree.viewTop = (groupTreeHeader.y + subTree.y) - groupTreeHeader.currentView.contentY
                }
            }

            ListView
            {
                id: subTreeList

                function clamp(min, v, max) { return Math.min(Math.max(v, min), max) }

                y: clamp(0, - parent.viewTop, parent.height)
                height: clamp(
                    0,
                    clamp(
                        0,
                        groupTreeHeader.currentView.height - parent.viewTop,
                        groupTreeHeader.currentView.height),
                    parent.height - y)

                width: parent ? parent.width : 0

                contentY: y

                model: RecursiveMembersModel
                {
                    id: recursiveMembers
                    membersCache: control.model.membersCache
                    groupId: groupTreeHeader.groupId
                }

                interactive: false
                enabled: false

                clip: true

                hoverHighlightColor: "transparent"

                delegate: SummaryItem {}
            }
        }
    }
}
