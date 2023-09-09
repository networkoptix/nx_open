// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import QtQuick.Shapes

import Qt5Compat.GraphicalEffects

import Nx
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
    property bool showDescription: false
    property alias editableFooter: allGroupsListView.footer

    property string summaryText: ""
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
                    return "image://svg/skin/user_settings/user_local.svg"
                case UserSettingsGlobal.TemporaryUser:
                    return "image://svg/skin/user_settings/user_local_temp.svg"
                case UserSettingsGlobal.CloudUser:
                    return "image://svg/skin/user_settings/user_cloud.svg"
                case UserSettingsGlobal.LdapUser:
                    return "image://svg/skin/user_settings/user_ldap.svg"
                default:
                    return ""
            }
        }
        if (model.groupSection == "B")
            return "image://svg/skin/user_settings/group_built_in.svg"

        return model.isLdap
            ? "image://svg/skin/user_settings/group_ldap.svg"
            : "image://svg/skin/user_settings/group_custom.svg"
    }

    component SelectableGroupItem: Rectangle
    {
        id: checkableItem

        property string text: ""
        property string description: ""

        property bool cycle: false

        readonly property bool selected: groupMouseArea.containsMouse || groupCheckbox.checked
        readonly property color selectedColor: selected
            ? ColorTheme.colors.light4
            : ColorTheme.colors.light10
        readonly property color descriptionColor: selected
            ? ColorTheme.colors.light10
            : ColorTheme.colors.light16

        height: 24

        color: groupMouseArea.containsMouse ? ColorTheme.colors.dark12 : "transparent"

        RowLayout
        {
            spacing: 0
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: 10
            anchors.right: parent.right

            Item
            {
                width: groupCheckbox.width
                height: groupCheckbox.height

                CheckBox
                {
                    id: groupCheckbox

                    checked: model[control.editableProperty]

                    enabled: control.enabledProperty ? model[control.enabledProperty] : true

                    baselineOffset: checkboxText.baselineOffset + checkboxText.y
                }

                // Setting 'hovered' property of groupCheckbox leads to QML crash.
                ColorOverlay
                {
                    anchors.fill: groupCheckbox
                    source: groupCheckbox
                    color: checkableItem.selectedColor
                }
            }

            Item
            {
                Layout.alignment: Qt.AlignVCenter

                width: groupImage.width
                height: groupImage.height

                IconImage
                {
                    id: groupImage

                    width: 20
                    height: 20

                    source: iconPath(model)
                    sourceSize: Qt.size(width, height)
                    color: checkableItem.selectedColor
                }
            }

            Text
            {
                id: checkboxText

                Layout.alignment: Qt.AlignVCenter
                Layout.fillWidth: true
                Layout.leftMargin: 2

                font: Qt.font({pixelSize: 12, weight: Font.Normal})
                color: checkableItem.cycle ? ColorTheme.colors.red_core : checkableItem.selectedColor
                elide: Text.ElideRight

                textFormat: Text.StyledText
                text:
                {
                    const result = highlightMatchingText(checkableItem.text)
                    if (control.currentSearchRegExp
                        || !control.showDescription
                        || !checkableItem.description)
                    {
                        return result
                    }

                    const description = NxGlobals.toHtmlEscaped(checkableItem.description)
                    return `${result}<font color="${checkableItem.descriptionColor}">`
                        + ` - ${description}</font>`
                }
            }
        }

        GlobalToolTip.text: model.toolTip || ""

        MouseArea
        {
            id: groupMouseArea
            anchors.fill: parent
            hoverEnabled: true
            onClicked:
            {
                model[control.editableProperty] = !model[control.editableProperty]
            }
        }
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

            header: Item
            {
                property alias text: searchField.text
                width: parent.width
                height: searchField.height + 16

                SearchField
                {
                    id: searchField
                    y: 16
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
            }

            delegate: SelectableGroupItem
            {
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
