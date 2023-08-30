// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import QtQuick.Layouts

import Nx
import Nx.Core
import Nx.Controls

import nx.client.desktop
import nx.vms.client.desktop

Control
{
    id: control

    property var model
    property var currentSearchRegExp: null
    readonly property color highlightColor: ColorTheme.colors.yellow_d1

    readonly property real kMinHeight: 28

    height: groupFlow.maxHeight

    onEnabledChanged: popupObject.close()

    function highlightMatchingText(text)
    {
        if (currentSearchRegExp)
            return NxGlobals.highlightMatch(text, currentSearchRegExp, highlightColor)

        return NxGlobals.toHtmlEscaped(text)
    }

    AllowedParentsModel
    {
        id: allowedParents
        sourceModel: control.model
    }

    ParentGroupsModel
    {
        id: parentGroupsModel

        directOnly: true
        sourceModel: control.model
    }

    background: Rectangle
    {
        color: popupObject.visible
            ? ColorTheme.colors.dark4
            : boxMouseArea.containsMouse
                ? ColorTheme.colors.dark6
                : ColorTheme.colors.dark5

        border.width: 1
        border.color: ColorTheme.darker(color, 1)

        opacity: control.enabled ? 1.0 : 0.3
    }

    contentItem: Item
    {
        id: boxContent

        Item
        {
            height: control.kMinHeight
            width: parent.width

            Text
            {
                anchors
                {
                    left: parent.left
                    leftMargin: 6
                    verticalCenter: parent.verticalCenter
                }
                text: control.enabled ? qsTr("Select") : ("<" + qsTr("No groups") + ">")

                opacity: control.enabled ? 1.0 : 0.3
                visible: model.parentGroups.length == 0
                color: ColorTheme.colors.light16
                font: Qt.font({pixelSize: 14, weight: Font.Normal})
            }
        }

        MouseArea
        {
            id: boxMouseArea
            anchors.fill: parent

            hoverEnabled: control.enabled
            onClicked: (mouse) =>
            {
                const clickY = mouse.y
                let showAbove = false
                popupObject.y = Qt.binding(() =>
                    {
                        // Open popup at line border below mouse click.
                        let popupY = control.height

                        const popupBottom = popupObject.parent.mapToGlobal(
                            0,
                            popupY + popupObject.height)

                        // Place popup above mouse click if the popup does not fit within window.
                        const w = control.Window.window
                        const windowBottomY = w.y + w.height

                        if (!popupObject.visible)
                            showAbove = popupBottom.y > windowBottomY

                        if (showAbove)
                            popupY -= (popupObject.height + control.height)

                        return popupY
                    })

                popupObject.open()
            }

            Image
            {
                opacity: control.enabled ? 0.4 : 1.0
                anchors
                {
                    right: parent.right
                    top: parent.top
                    rightMargin: 6
                    topMargin: 6
                }
                width: 16
                height: 16
                sourceSize: Qt.size(width, height)
                source: "image://svg/skin/user_settings/arrow_down.svg"
            }

            LimitedFlow
            {
                id: groupFlow

                spacing: 2
                leftPadding: 4
                rightPadding: 4
                topPadding: 3
                bottomPadding: 3

                anchors
                {
                    left: parent.left
                    right: parent.right
                    rightMargin: 34
                }

                sourceModel: parentGroupsModel

                Rectangle
                {
                    id: moreGroupsButton

                    width: innerText.width + 8 * 2
                    height: 22
                    radius: 2

                    border
                    {
                        width: 1
                        color: ColorTheme.colors.dark12
                    }
                    color: ColorTheme.colors.dark7

                    Text
                    {
                        id: innerText

                        anchors.centerIn: parent
                        color: ColorTheme.colors.light10
                        text: qsTr(`${groupFlow.remaining} more`)
                    }
                }

                delegate: Rectangle
                {
                    implicitWidth: groupText.implicitWidth + 20
                        + (model.canEditMembers ? 20 : 7) + 4 + 4 + 1

                    width: Math.min(
                        implicitWidth,
                        groupFlow.width - 8)

                    height: 22
                    radius: 2
                    border
                    {
                        width: 1
                        color: ColorTheme.colors.dark12
                    }

                    color: ColorTheme.colors.dark10

                    RowLayout
                    {
                        id: groupRow

                        anchors
                        {
                            verticalCenter: parent.verticalCenter
                            left: parent.left
                            right: parent.right
                        }

                        spacing: 0

                        Image
                        {
                            Layout.leftMargin: 4
                            Layout.alignment: Qt.AlignVCenter

                            width: 20
                            height: 20

                            source:
                            {
                                if (model.isPredefined)
                                    return "image://svg/skin/user_settings/group_built_in.svg"

                                return model.isLdap
                                    ? "image://svg/skin/user_settings/group_ldap.svg"
                                    : "image://svg/skin/user_settings/group_custom.svg"
                            }

                            sourceSize: Qt.size(width, height)
                        }

                        Text
                        {
                            id: groupText

                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignVCenter
                            Layout.rightMargin: model.canEditMembers ? 0 : 7

                            elide: Text.ElideRight
                            text: model.text
                            color: ColorTheme.colors.light4
                            font: Qt.font({pixelSize: 14, weight: Font.Normal})
                        }

                        ImageButton
                        {
                            visible: model.canEditMembers

                            Layout.rightMargin: 4
                            Layout.alignment: Qt.AlignVCenter

                            width: 20
                            height: 20

                            icon.source: "image://svg/skin/user_settings/cross_input.svg"
                            icon.width: width
                            icon.height: height
                            icon.color: ColorTheme.colors.light10

                            onClicked: model.isParent = false
                        }
                    }
                }
            }
        }
    }

    Popup
    {
        id: popupObject

        width: control.width
        height: popupContent.implicitHeight

        padding: 0
        topPadding: 2
        bottomPadding: 2

        background: Rectangle
        {
            id: popupBack

            color: ColorTheme.colors.dark13
            radius: 1
        }

        PopupShadow
        {
            source: popupBack
        }

        onVisibleChanged:
        {
            searchField.focus = visible
            if (searchField.focus)
                searchField.forceActiveFocus()

            popupContent.groupCount = allowedParents.rowCount()
        }

        contentItem: Item
        {
            id: popupContent

            property int groupCount: 0
            onGroupCountChanged: implicitHeight = computeHeight(groupCount)
            implicitHeight: computeHeight(groupCount)

            function computeHeight(count)
            {
                return 46 + 28 * Math.max(1, Math.min(count, 5)) + 7 + 4
            }

            TextField
            {
                id: searchField

                leftPadding: 26
                rightPadding: 26

                placeholderText: qsTr("Search")
                placeholderTextColor: searchField.hovered || searchField.activeFocus
                    ? ColorTheme.colors.light10
                    : ColorTheme.colors.light13

                ImageButton
                {
                    id: clearInput

                    visible: !!searchField.text

                    anchors
                    {
                        verticalCenter: parent.verticalCenter
                        right: parent.right
                        rightMargin: 6
                    }

                    width: 16
                    height: 16

                    icon.source: "image://svg/skin/user_settings/clear_text_field.svg"
                    icon.width: width
                    icon.height: height

                    onClicked: searchField.text = ""
                }

                onAccepted:
                {
                    if (groupListView.count == 1)
                    {
                        groupListView.itemAtIndex(0).setChecked(true)
                        popupObject.close()
                    }
                }

                onTextChanged:
                {
                    // For text highlight.
                    currentSearchRegExp = text
                        ? new RegExp(`(${NxGlobals.makeSearchRegExpNoAnchors(text)})`, 'i')
                        : ""
                    allowedParents.setFilterRegularExpression(currentSearchRegExp)
                    popupContent.groupCount = allowedParents.rowCount()
                }

                anchors
                {
                    top: parent.top
                    topMargin: 10
                    left: parent.left
                    leftMargin: 8
                    right: parent.right
                    rightMargin: 8
                }

                color:
                {
                    if (searchField.activeFocus)
                        return ColorTheme.colors.light4

                    if (searchField.hovered)
                        return ColorTheme.colors.light10

                    return ColorTheme.colors.light13
                }

                background: Rectangle
                {
                    IconImage
                    {
                        x: 6
                        anchors.verticalCenter: parent.verticalCenter

                        sourceSize: Qt.size(16, 16)
                        source: "image://svg/skin/user_settings/search.svg"
                        color: searchField.color
                    }

                    radius: 2
                    color: searchField.activeFocus
                        ? ColorTheme.colors.dark8
                        : ColorTheme.colors.dark10

                    border.width: 1
                    border.color: ColorTheme.colors.dark9
                }
            }

            Text
            {
                anchors.fill: groupListView
                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: Text.AlignHCenter
                leftPadding: 16
                rightPadding: 16
                wrapMode: Text.WordWrap

                visible: groupListView.count == 0

                text: qsTr("Nothing found")

                color: ColorTheme.colors.light16
                font: Qt.font({pixelSize: 14, weight: Font.Normal})
            }

            ListView
            {
                id: groupListView

                anchors.fill: parent
                anchors.topMargin: 46

                clip: true

                scrollBar.width: 8
                scrollBar.thumbColor: ColorTheme.colors.dark16

                model: allowedParents

                section.property: "groupSection"

                visible: groupListView.count > 0

                delegate: Item
                {
                    readonly property bool isSectionBorder:
                        ListView.previousSection != ListView.section && ListView.previousSection

                    function setChecked(value)
                    {
                        model.isParent = value
                    }

                    width: parent
                        ? (parent.width - (groupListView.scrollBar.visible
                            ? groupListView.scrollBar.width
                            : 0))
                        : 30

                    height: 28 + (isSectionBorder ? sectionBorder.height : 0)

                    Rectangle
                    {
                        id: sectionBorder

                        visible: parent.isSectionBorder
                        height:7
                        clip: true
                        width: parent.width
                        color: ColorTheme.colors.dark13

                        Rectangle
                        {
                            anchors
                            {
                                left: parent.left
                                right: parent.right
                                leftMargin: 6
                                rightMargin: 6
                                verticalCenter: parent.verticalCenter
                            }
                            height: 1
                            color: ColorTheme.colors.dark11
                        }
                    }

                    Rectangle
                    {

                        anchors
                        {
                            left: parent.left
                            right: parent.right
                            bottom: parent.bottom
                        }

                        height: 28

                        color: groupMouseArea.containsMouse
                            ? ColorTheme.colors.dark15
                            : ColorTheme.colors.dark13

                        RowLayout
                        {
                            spacing: 2

                            anchors.fill: parent
                            anchors.topMargin: parent.isSectionBorder ? sectionBorder.height : 0
                            anchors.leftMargin: 6
                            anchors.rightMargin: 6

                            Image
                            {
                                id: groupImage

                                width: 20
                                height: 20

                                source:
                                {
                                    if (model.groupSection == "B")
                                        return "image://svg/skin/user_settings/group_built_in.svg"

                                    return model.isLdap
                                        ? "image://svg/skin/user_settings/group_ldap.svg"
                                        : "image://svg/skin/user_settings/group_custom.svg"
                                }

                                sourceSize: Qt.size(width, height)
                            }

                            Text
                            {
                                color: model.isParent
                                    ? ColorTheme.colors.light1
                                    : ColorTheme.colors.light4
                                font: Qt.font({pixelSize: 14, weight: Font.Normal})
                                Layout.fillWidth: true

                                elide: Text.ElideRight

                                textFormat: Text.StyledText
                                text: highlightMatchingText(model.text)
                            }

                            CheckBox
                            {
                                id: groupCheckbox

                                checked: model.isParent

                                Layout.alignment: Qt.AlignVCenter
                                Layout.topMargin: 6
                            }
                        }

                        MouseArea
                        {
                            id: groupMouseArea
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked:
                            {
                                model.isParent = !model.isParent
                            }
                        }
                    }
                }
            }
        }
    }
}
