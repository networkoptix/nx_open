// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls

import Nx.Common
import Nx.Core
import Nx.Core.Controls
import Nx.Ui

import nx.vms.client.core

TreeViewDelegate
{
    id: delegateItem

    implicitWidth: treeView.width
    implicitHeight: 40
    background: null

    property var highlightRegExp

    readonly property Resource resource: (model && model.resource) || null

    readonly property bool isCamera: resourceHelper.isCamera
    readonly property bool isLayout: resourceHelper.isLayout
    readonly property bool isAllDevices: model.nodeType === ResourceTree.NodeType.allDevices

    readonly property int flags: (model && model.resourceExtraStatus) || 0
    readonly property string iconSource:
    {
        if (!model)
            return ""

        return (model.iconKey && model.iconKey !== 0
            && ("image://resource/" + model.iconKey)) || ""
    }

    readonly property bool isCurrent:
    {
        if (hasChildren)
            return false;

        if (resourceHelper.isLayout)
            return windowContext.deprecatedUiController.layout === resource

        if (model.nodeType === ResourceTree.NodeType.allDevices)
            return windowContext.deprecatedUiController.layout === null

        return false
    }

    ResourceHelper
    {
        id: resourceHelper
        resource: delegateItem.resource
    }

    Rectangle
    {
        anchors.fill: parent
        z: -1
        color: ColorTheme.colors.dark9
        radius: 4
        visible: isCurrent
    }

    contentItem: Item
    {
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.right: parent.right

        Row
        {
            id: contentRow

            anchors.verticalCenter: parent.verticalCenter
            spacing: 4

            Image
            {
                anchors.verticalCenter: parent.verticalCenter
                width: 20
                height: 20
                sourceSize: Qt.size(20, 20)
                source: iconSource
            }
            Label
            {
                textFormat: Text.RichText
                anchors.verticalCenter: parent.verticalCenter
                text: highlightRegExp
                    ? NxGlobals.highlightMatch(model.display, highlightRegExp, ColorTheme.colors.yellow_l)
                    : model.display
                horizontalAlignment: Text.AlignLeft
                color: ColorTheme.colors.light10
                font.pixelSize: 14
                font.family: "Roboto"
                font.weight: 400
            }
        }

        RecordingStatusIndicator
        {
            anchors.right: contentRow.left
            anchors.verticalCenter: parent.verticalCenter
            width: 20
            height: 20
            id: recordingIcon
            resource: delegateItem.resource
            color: (delegateItem.flags & ResourceTree.ResourceExtraStatusFlag.recording
                || delegateItem.flags & ResourceTree.ResourceExtraStatusFlag.scheduled)
                ? ColorTheme.colors.red_l
                : ColorTheme.colors.dark17
        }
    }

    indicator: ColoredImage
    {
        x: leftMargin + (depth * indentation)
        anchors.verticalCenter: parent.verticalCenter
        width: 20
        height: 20
        sourcePath: expanded
            ? "image://skin/20x20/Solid/arrow_open.svg"
            : "image://skin/20x20/Solid/arrow_right.svg"
        primaryColor: ColorTheme.colors.dark17
    }
}
