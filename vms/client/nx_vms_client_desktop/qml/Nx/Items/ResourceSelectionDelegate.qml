// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Common
import Nx.Controls
import Nx.Core
import Nx.Core.Controls

import nx.vms.client.core
import nx.vms.client.desktop

Item
{
    id: delegateItem

    required property int selectionMode
    required property bool showExtraInfo
    required property bool showResourceStatus

    property bool customSelectionIndicator: false
    property bool wholeRowToggleable: true //< Clicks on any place of a row toggle selection.

    property var nextCheckState: //< Not used in exclusive selection mode.
        (checkState => checkState === Qt.Checked ? Qt.Unchecked : Qt.Checked)

    readonly property Resource resource: (model && model.resource) || null
    readonly property int nodeType: (model && model.nodeType) || -1

    readonly property bool isSeparator: nodeType == ResourceTree.NodeType.separator
        || nodeType == ResourceTree.NodeType.localSeparator

    readonly property real availableWidth: width - indicator.width

    readonly property color highlightColor: ColorTheme.colors.yellow_d1
    property var highlightRegExp: null

    implicitHeight: 20 //< Just a sensible default.

    implicitWidth: isSeparator
        ? 0
        : (contentRow.implicitWidth + indicator.width + contentRow.spacing)

    baselineOffset: contentRow.y + name.y + name.baselineOffset

    function highlightMatchingText(text)
    {
        const escaped = text
            .replace(/&/g, "&amp;")
            .replace(/</g, "&lt;")

        if (!highlightRegExp)
            return escaped

        return escaped.replace(highlightRegExp, `<font color="${highlightColor}">$1</font>`)
    }

    Row
    {
        id: contentRow

        height: delegateItem.height
        spacing: 4

        Image
        {
            id: icon

            anchors.verticalCenter: contentRow.verticalCenter

            readonly property int imageState: delegateItem.highlighted
                ? ResourceTree.ItemState.selected
                : ResourceTree.ItemState.normal

            source: (model && model.iconKey)
                ? `image://resource/${model.iconKey}/${imageState}`
                : ""

            sourceSize: Qt.size(20, 20)

            width: 20
            height: 20

            Row
            {
                id: extras

                spacing: 0
                height: parent.height
                x: -(width + (NxGlobals.hasChildren(modelIndex) ? 20 : 0))
                visible: delegateItem.showResourceStatus

                readonly property int flags: (model && model.resourceExtraStatus) || 0

                ColoredImage
                {
                    id: problemsIcon
                    visible: extras.flags & ResourceTree.ResourceExtraStatusFlag.buggy
                    sourcePath: "image://skin/20x20/Solid/locked.svg"
                    sourceSize: Qt.size(20, 20)
                    primaryColor: "light4"
                }

                RecordingStatusIndicator
                {
                    id: recordingIcon
                    resource: delegateItem.resource
                    color: (extras.flags & ResourceTree.ResourceExtraStatusFlag.recording
                        || extras.flags & ResourceTree.ResourceExtraStatusFlag.scheduled)
                        ? "red_l"
                        : "dark17"
                }
            }
        }

        Text
        {
            id: name

            text: (model && highlightMatchingText(model.display)) || ""
            textFormat: Text.StyledText
            font: FontConfig.resourceTree
            height: parent.height
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight

            width: Math.min(implicitWidth, delegateItem.availableWidth - x)
            color: delegateItem.color
        }

        Text
        {
            id: extraInfo

            text: (delegateItem.showExtraInfo && model && model.extraInfo) || ""
            textFormat: Text.PlainText
            font.weight: Font.Normal
            height: parent.height
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
            leftPadding: 1

            width: Math.min(implicitWidth, delegateItem.availableWidth - x)
            color: delegateItem.extraColor
        }
    }

    Row
    {
        id: indicator

        leftPadding: 8
        rightPadding: 8

        visible: !!indicatorLoader.item
        width: indicatorLoader.item ? implicitWidth : 0
        height: delegateItem.height
        x: delegateItem.width - width

        readonly property int checkState: (model && model.checkState) ?? 0

        Loader
        {
            id: indicatorLoader

            active: !delegateItem.isSeparator
            baselineOffset: name.baselineOffset

            Component
            {
                id: checkBoxComponent

                CheckBoxImage
                {
                    checkState: indicator.checkState
                    anchors.baseline: indicatorLoader.baseline
                }
            }

            Component
            {
                id: radioButtonComponent

                RadioButtonImage
                {
                    checked: indicator.checkState === Qt.Checked
                    anchors.baseline: indicatorLoader.baseline
                }
            }

            sourceComponent:
            {
                if (delegateItem.customSelectionIndicator)
                    return null

                switch (delegateItem.selectionMode)
                {
                    case ResourceTree.ResourceSelection.single:
                        return delegateItem.resource ? checkBoxComponent : null

                    case ResourceTree.ResourceSelection.multiple:
                        return checkBoxComponent

                    case ResourceTree.ResourceSelection.exclusive:
                        return delegateItem.resource ? radioButtonComponent : null

                    default:
                        return null
                }
            }
        }
    }

    MouseArea
    {
        id: checkStateToggler

        anchors.fill: delegateItem
        acceptedButtons: Qt.LeftButton
        visible: !delegateItem.isSeparator && delegateItem.wholeRowToggleable

        onClicked:
        {
            model.checkState = selectionMode === ResourceTree.ResourceSelection.exclusive
                ? Qt.Checked
                : delegateItem.nextCheckState(indicator.checkState)
        }
    }

    Rectangle
    {
        id: separatorLine

        height: 1
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: -itemIndent

        color: ColorTheme.transparent(ColorTheme.colors.dark8, 0.4)
        visible: delegateItem.isSeparator
    }

    property bool highlighted: indicator.checkState !== Qt.Unchecked

    readonly property color color: highlighted
        ? ColorTheme.colors.light4
        : ColorTheme.colors.light10

    readonly property color extraColor: highlighted
        ? ColorTheme.colors.light10
        : (isSelected ? ColorTheme.colors.light14 : ColorTheme.colors.dark17)
}
