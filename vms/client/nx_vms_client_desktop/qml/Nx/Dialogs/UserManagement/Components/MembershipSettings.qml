// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import QtQuick.Shapes

import Nx.Core
import Nx.Controls

import nx.vms.client.desktop

Item
{
    id: control

    property var currentSearchRegExp: null

    property string editableProperty: "checked"
    property string enabledProperty: ""

    property bool editable: true
    property bool enabled: true

    property alias editableModel: allGroupsListView.model
    property alias editableSection: allGroupsListView.section
    property alias editableDelegate: allGroupsListView.delegate
    property bool showDescription: false

    property string summaryText: ""
    property string infoText: ""
    property alias summaryModel: selectedGroupsListView.model
    property alias summarySection: selectedGroupsListView.section
    property alias summaryDelegate: selectedGroupsListView.delegate

    property alias editablePlaceholder: editablePlaceholderItem.children

    property var summaryPlaceholder: []

    readonly property color highlightColor: ColorTheme.colors.yellow_d1

    function highlightMatchingText(text)
    {
        if (currentSearchRegExp)
            return NxGlobals.highlightMatch(text, currentSearchRegExp, highlightColor)

        return NxGlobals.toHtmlEscaped(text)
    }

    function iconPath(model)
    {
        if (model.isUser)
        {
            switch (model.userType)
            {
                case UserSettingsGlobal.LocalUser:
                    return "image://skin/20x20/Solid/user.svg"
                case UserSettingsGlobal.TemporaryUser:
                    return "image://skin/20x20/Solid/user_temp.svg"
                case UserSettingsGlobal.CloudUser:
                    return "image://skin/20x20/Solid/user_cloud.svg"
                case UserSettingsGlobal.LdapUser:
                    return "image://skin/20x20/Solid/user_ldap.svg"
                case UserSettingsGlobal.OrganizationUser:
                    return "image://skin/20x20/Solid/user_organization.svg"
                case UserSettingsGlobal.ChannelPartnerUser:
                    return "image://skin/20x20/Solid/user_cp.svg"
                default:
                    return ""
            }
        }

        if (model.isPredefined)
            return "image://skin/20x20/Solid/group_default.svg"

        return model.isLdap
            ? "image://skin/20x20/Solid/group_ldap.svg"
            : "image://skin/20x20/Solid/group.svg"
    }

    component SelectableGroupItem: MembershipEditableItem
    {
        text: model.text
        description: model.description
        cycle: model.cycle
    }

    Item
    {
        id: editablePanel

        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.leftMargin: 16

        width: 0.4 * parent.width
        clip: true
        visible: control.editable

        ListView
        {
            id: allGroupsListView
            anchors.fill: parent

            enabled: control.enabled

            header: Column
            {
                property alias text: searchField.text
                width: parent.width
                spacing: 8
                topPadding: 16

                SearchField
                {
                    id: searchField
                    width: parent.width - 16

                    onTextChanged:
                    {
                        // For text highlight.
                        currentSearchRegExp = text
                            ? new RegExp(`(${NxGlobals.makeSearchRegExpNoAnchors(text)})`, 'i')
                            : ""
                        allGroupsListView.model.setFilterRegularExpression(currentSearchRegExp)
                        selectedGroupsListView.model.setFilterRegularExpression(currentSearchRegExp)
                    }
                }

                Text
                {
                    id: infoTextLabel

                    text: control.infoText
                    visible: !!text
                    width: parent.width - 16
                    wrapMode: Text.WordWrap
                    color: ColorTheme.colors.light16
                }
            }

            delegate: SelectableGroupItem
            {
                view: ListView.view
                width: parent ? parent.width : 0
                text: model.text
                description: model.description
                cycle: model.cycle
            }

            hoverHighlightColor: "transparent"

            section.property: control.checkSectionProperty
            section.criteria: ViewSection.FullString

            section.delegate: SectionHeader
            {
                text: section
            }
        }

        Item
        {
            id: editablePlaceholderItem

            visible: allGroupsListView.count === 0 && editablePlaceholderItem.children.length

            anchors.bottom: parent.bottom

            x: 0
            width: parent.width - editablePanel.anchors.leftMargin
            height: parent.height
                - (allGroupsListView.headerItem ? allGroupsListView.headerItem.height : 0)
        }
    }

    Rectangle
    {
        anchors.left: editablePanel.right
        height: parent.height
        width: 1
        color: ColorTheme.colors.dark6
        visible: editablePanel.visible
    }

    Rectangle
    {
        id: summaryPanel

        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        anchors.left: editablePanel.visible ? editablePanel.right : parent.left
        anchors.leftMargin: editablePanel.visible ? 1 : 0

        color: ColorTheme.colors.dark8

        Placeholder
        {
            visible: selectedGroupsListView.count === 0 && !editablePlaceholderItem.visible

            anchors.centerIn: parent
            anchors.verticalCenterOffset: additionalText ? -100 : 0

            imageSource: control.summaryPlaceholder[0]
            text: control.summaryPlaceholder[1]
            additionalText: control.summaryPlaceholder[2]
        }

        ListView
        {
            id: selectedGroupsListView

            anchors.fill: parent
            anchors.leftMargin: 16

            visible: count > 0
            clip: true
            hoverHighlightColor: "transparent"

            enabled: control.enabled

            header: Item
            {
                width: parent.width - 16
                height: summaryHeader.height

                SectionHeader
                {
                    id: summaryHeader
                    text: control.summaryText
                }
            }

            delegate: MembershipTreeItem
            {
                enabled: control.enabled
                offset: model.offset
            }
        }
    }
}
