// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import QtQuick.Layouts

import Nx.Core
import Nx.Core.Controls
import Nx.Controls

import nx.vms.client.desktop

MultiSelectionComboBox
{
    id: control

    property alias groupsModel: allowedParents.sourceModel

    enabledRole: "canEditMembers"
    checkedRole: "isParent"
    decorationRole: "decorationPath"
    placeholder.text: enabled ? qsTr("Select") : ("<" + qsTr("No groups") + ">")
    closePopupWhenClicked: true

    model: AllowedParentsModel
    {
        id: allowedParents

        property int count: 0 //< Required by MultiSelectionComboBox.

        onRowsInserted: count = rowCount()
        onRowsRemoved: count = rowCount()
        onModelReset: count = rowCount()
    }

    listView.Layout.maximumHeight: 200
    listView.section.property: "groupSection"
    listView.section.delegate: Item
    {
        id: groupSection
        height: 28
        width: parent ? parent.width : groupSectionText.width
        baselineOffset: groupSectionText.y + groupSectionText.baselineOffset

        Text
        {
            id: groupSectionText

            anchors.top: parent.top
            anchors.topMargin: 8
            anchors.left: parent.left
            anchors.leftMargin: 8

            height: 12
            font: Qt.font({pixelSize: 10, weight: Font.Medium})
            color: ColorTheme.colors.light10

            text:
            {
                if (section == UserSettingsGlobal.kLdapGroupsSection)
                    return qsTr("LDAP", "Acronym for The Lightweight Directory Access Protocol")

                if (section == UserSettingsGlobal.kOrgGroupsSection)
                    return qsTr("ORGANIZATION", "Section with groups from organisation")

                return section === UserSettingsGlobal.kBuiltInGroupsSection
                    ? qsTr("BUILT-IN", "Section name in a list of items: 'Built-in groups'")
                    : qsTr("CUSTOM", "Section name in a list of items: 'Custom groups'")
            }
        }

        Rectangle
        {
            anchors
            {
                bottom: parent.bottom
                bottomMargin: 4
                left: parent.left
                leftMargin: 8
                right: parent.right
                rightMargin: 8
            }
            height: 1
            width: parent.width
            color: ColorTheme.colors.dark11
        }
    }
}
