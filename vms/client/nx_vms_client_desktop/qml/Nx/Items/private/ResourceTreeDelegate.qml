// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6

import Nx 1.0
import Nx.Controls 1.0

import nx.vms.client.core 1.0
import nx.vms.client.desktop 1.0

FocusScope
{
    id: delegateItem

    required property bool showExtraInfo
    required property int itemState //< ResourceTree.ItemState enumeration

    readonly property var resource: (model && model.resource) || null
    readonly property int nodeType: (model && model.nodeType) || -1

    readonly property bool isSeparator: nodeType == ResourceTree.NodeType.separator
        || nodeType == ResourceTree.NodeType.localSeparator

    property real availableWidth: width
    property bool isEditing: false

    focus: isEditing

    implicitHeight: 20 //< Some sensible default.
    implicitWidth: isSeparator ? 0 : contentRow.implicitWidth

    Row
    {
        id: contentRow

        height: delegateItem.height
        spacing: 4

        Image
        {
            id: icon

            anchors.verticalCenter: contentRow.verticalCenter

            source: iconSource
            sourceSize: Qt.size(20, 20)

            Row
            {
                id: extras

                spacing: 0
                height: parent.height
                x: -(width + (NxGlobals.hasChildren(modelIndex) ? 20 : 0))

                readonly property int flags: (model && model.resourceExtraStatus) || 0

                Image
                {
                    id: problemsIcon
                    visible: extras.flags & ResourceTree.ResourceExtraStatusFlag.locked
                    source: "qrc:///skin/tree/locked.svg"
                }

                Image
                {
                    id: recordingIcon

                    source:
                    {
                        if (extras.flags & ResourceTree.ResourceExtraStatusFlag.recording)
                            return "qrc:///skin/tree/recording.png"

                        if (extras.flags & ResourceTree.ResourceExtraStatusFlag.scheduled)
                            return "qrc:///skin/tree/scheduled.png"

                        if (extras.flags & ResourceTree.ResourceExtraStatusFlag.hasArchive)
                            return "qrc:///skin/tree/has_archive.png"

                        return ""
                    }
                }
            }
        }

        Text
        {
            id: name

            text: (model && model.display) || ""
            textFormat: Text.PlainText
            font.weight: Font.DemiBold
            height: parent.height
            verticalAlignment: Text.AlignVCenter
            visible: !delegateItem.isEditing
            width: Math.min(implicitWidth, delegateItem.availableWidth - x)
            elide: Text.ElideRight
            color: delegateItem.mainTextColor
        }

        Text
        {
            id: extraInfo

            text: (delegateItem.showExtraInfo && model && model.extraInfo) || ""
            textFormat: Text.PlainText
            font.weight: Font.Normal
            height: parent.height
            width: Math.min(implicitWidth, delegateItem.availableWidth - x)
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
            visible: !delegateItem.isEditing
            leftPadding: 1

            color:
            {
                switch (itemState)
                {
                    case ResourceTree.ItemState.accented:
                        return ColorTheme.colors.brand_d3
                    case ResourceTree.ItemState.selected:
                        return ColorTheme.colors.light10
                    default:
                        return ColorTheme.colors.dark17
                }
            }
        }

        TextInput
        {
            id: nameEditor

            font.weight: Font.DemiBold
            height: parent.height
            width: delegateItem.width - x - 1
            verticalAlignment: Text.AlignVCenter
            selectionColor: selectionHighlightColor
            selectByMouse: true
            selectedTextColor: delegateItem.mainTextColor
            color: delegateItem.mainTextColor
            visible: delegateItem.isEditing
            focus: true
            clip: true

            onVisibleChanged:
            {
                if (!visible)
                    return

                forceActiveFocus()
                selectAll()
            }

            Keys.onPressed:
            {
                event.accepted = true
                switch (event.key)
                {
                    case Qt.Key_Enter:
                    case Qt.Key_Return:
                        nameEditor.commit()
                        break

                    case Qt.Key_Escape:
                        nameEditor.revert()
                        break

                    default:
                        event.accepted = false
                        break
                }
            }

            Keys.onShortcutOverride:
            {
                switch (event.key)
                {
                    case Qt.Key_Enter:
                    case Qt.Key_Return:
                    case Qt.Key_Escape:
                        event.accepted = true
                        break
                }
            }

            onEditingFinished:
                commit()

            function edit()
            {
                if (!model)
                    return

                text = model.display || ""
                delegateItem.isEditing = true
            }

            function commit()
            {
                if (!delegateItem.isEditing)
                    return

                delegateItem.isEditing = false

                if (!model)
                    return

                const trimmedName = text.trim().replace('\n','')
                if (trimmedName)
                    model.edit = trimmedName
            }

            function revert()
            {
                delegateItem.isEditing = false
            }

            Connections
            {
                target: delegateItem.parent

                function onStartEditing() { nameEditor.edit() }
                function onFinishEditing() { nameEditor.commit() }
            }
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

    // Never pass key presses to parents while editing.
    Keys.onPressed:
        event.accepted = isEditing

    readonly property string iconSource:
    {
        if (!model)
            return ""

        if (model.decorationPath)
            return "qrc:/skin/" + model.decorationPath

        return (model.iconKey && model.iconKey !== 0
            && ("image://resource/" + model.iconKey + "/" + itemState)) || ""
    }

    property color mainTextColor:
    {
        switch (itemState)
        {
            case ResourceTree.ItemState.accented:
                return ColorTheme.colors.brand_core
            case ResourceTree.ItemState.selected:
                return ColorTheme.colors.light4
            default:
                return ColorTheme.colors.light10
        }
    }
}
