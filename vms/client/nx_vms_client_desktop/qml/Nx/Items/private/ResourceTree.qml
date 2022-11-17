// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6
import QtQuick.Window 2.14

import Nx 1.0
import Nx.Common 1.0
import Nx.Controls 1.0
import Nx.Layout 1.0
import Nx.Models 1.0
import Nx.Utils 1.0

import nx.vms.client.core 1.0
import nx.vms.client.desktop 1.0

TreeView
{
    id: resourceTree

    readonly property real kRowHeight: 20
    readonly property real kSeparatorRowHeight: 16

    property var scene: null

    property alias filterText: resourceTreeModel.filterText
    property alias filterType: resourceTreeModel.filterType
    property alias isFiltering: resourceTreeModel.isFiltering
    property alias localFilesMode: resourceTreeModel.localFilesMode

    property alias toolTipVideoDelayMs: toolTip.videoDelayMs //< Disabled if zero or negative.
    property alias toolTipVideoQuality: toolTip.videoQuality

    property rect toolTipEnclosingRect: Qt.rect(width, 0, Number.MAX_VALUE / 2, height)

    property bool shortcutHintsEnabled: true

    readonly property bool isEditing: currentItem ? currentItem.isEditing : false

    property var stateStorage: null

    function isFilterRelevant(type)
    {
        return model.isFilterRelevant(type)
    }

    topMargin: 8
    leftMargin: rootIsDecorated ? -2 : 2
    indentation: 20
    scrollStepSize: kRowHeight
    hoverHighlightColor: ColorTheme.transparent(ColorTheme.colors.dark8, 0.4)
    selectionHighlightColor: ColorTheme.transparent(ColorTheme.colors.brand_core, 0.3)
    dropHighlightColor: ColorTheme.transparent(ColorTheme.colors.brand_core, 0.6)
    rootIndex: resourceTreeModel.rootIndex
    autoExpandRoleName: "autoExpand"

    expandsOnDoubleClick: (modelIndex => resourceTreeModel.expandsOnDoubleClick(modelIndex))
    activateOnSingleClick: (modelIndex => resourceTreeModel.activateOnSingleClick(modelIndex))

    rootIsDecorated:
    {
        return filterType == ResourceTree.FilterType.noFilter
            || filterType == ResourceTree.FilterType.everything
            || filterType == ResourceTree.FilterType.cameras
            || filterType == ResourceTree.FilterType.localFiles
    }

    onContextMenuRequested:
        resourceTreeModel.showContextMenu(globalPos, modelIndex, selection)

    onActivated:
        resourceTreeModel.activateItem(modelIndex, selection, activationType, modifiers)

    model: ResourceTreeModel
    {
        id: resourceTreeModel

        context: workbenchContext

        onEditRequested:
            resourceTree.startEditing()

        onFilterAboutToBeChanged:
        {
            if (!isFiltering)
                pushState(resourceTree.expandedState())
        }

        onFilterChanged:
        {
            if (isFiltering)
                resourceTree.expandAll()
            else
                resourceTree.setExpandedState(popState())
        }

        function acquireExpandedState(expandedIndexes, prevState)
        {
            const state = {
                // Maintain expanded servers state when they are hidden.
                // See releaseExpandedState() below.
                "expandedResources": prevState ? prevState.expandedResources : new Set(),
                "expandedGroups": new Set(),
                "expandedServers": new Set(),
                "expandedCameraGroups": new Set(),
                "expandedTopLevelRows": new Set(),
                "topLevelRows": []
            }

            const kTopLevelTypes = [
                ResourceTree.NodeType.servers,
                ResourceTree.NodeType.camerasAndDevices]

            for (let index of expandedIndexes)
            {
                // Only select indexes under top level "Servers" or "Cameras & Devices".
                let topLevel = index
                while (topLevel.parent != resourceTreeModel.rootIndex)
                    topLevel = topLevel.parent

                const nodeType = Number(modelDataAccessor.getData(topLevel, "nodeType"))
                if (!kTopLevelTypes.includes(nodeType))
                    continue

                // "Servers" and "Cameras & Devices" share the same row number.
                state.topLevelRows[0] = topLevel.row
                if (index.parent == resourceTreeModel.rootIndex)
                    state.expandedTopLevelRows.add(index.row)

                const customId = modelDataAccessor.getData(index, "customGroupId")
                if (customId)
                {
                    state.expandedGroups.add(customId)
                    continue
                }

                const cameraGroupId = modelDataAccessor.getData(index, "cameraGroupId")
                if (cameraGroupId)
                {
                    state.expandedCameraGroups.add(cameraGroupId)
                    continue
                }

                const resource = modelDataAccessor.getData(index, "resource")
                if (resource)
                {
                    state.expandedResources.add(resource.id)

                    if (resource.flags & NxGlobals.ServerResourceFlag)
                        state.expandedServers.add(resource.id)
                }
            }

            return state
        }

        function expandedStateCallback(state)
        {
            return (index) =>
                {
                    if (index.parent == resourceTreeModel.rootIndex)
                        return state.expandedTopLevelRows.has(index.row)

                    const customId = modelDataAccessor.getData(index, "customGroupId")
                    if (state.expandedGroups.has(customId))
                        return true

                    const cameraGroupId = modelDataAccessor.getData(index, "cameraGroupId")
                    if (state.expandedCameraGroups.has(cameraGroupId))
                        return true

                    const resource = modelDataAccessor.getData(index, "resource")
                    if (resource && state.expandedResources.has(resource.id))
                        return true

                    return false
                }
        }

        function releaseExpandedState(prevState)
        {
            // Maintain expanded servers state when they are hidden.
            // Servers state is going to be restored because expandedResources
            // is carried on from the previous state.
            // Remember that we toggle only between two states:
            //   Servers
            //   Cameras & Devices
            return {
                "expandedResources": prevState.expandedServers
            }
        }

        onSaveExpandedState:
        {
            // Convert selectedIndexes to QVariantList to avoid huge performance loss.
            const expandedIndexes = NxGlobals.toVariantList(resourceTree.expandedState())
            stateStorage = acquireExpandedState(expandedIndexes, stateStorage)
        }

        onRestoreExpandedState:
        {
            resourceTree.expand(expandedStateCallback(stateStorage), stateStorage.topLevelRows)
            stateStorage = releaseExpandedState(stateStorage)
        }
    }

    delegate: Component
    {
        FocusScope
        {
            id: delegateItem

            readonly property int nodeType: (model && model.nodeType) || -1

            readonly property var resource: (model && model.resource) || null

            readonly property AbstractResourceThumbnail thumbnailSource: thumbnail

            readonly property bool isSeparator: nodeType == ResourceTree.NodeType.separator
                || nodeType == ResourceTree.NodeType.localSeparator

            property bool isEditing: false

            focus: isEditing

            implicitHeight: isSeparator ? kSeparatorRowHeight : kRowHeight
            implicitWidth: isSeparator ? 0 : contentRow.implicitWidth

            ResourceIdentificationThumbnail
            {
                id: thumbnail

                resource: delegateItem.resource
                maximumSize: 400
                obsolescenceMinutes: 15
                enforced: resourceTree.hoveredItem === delegateItem

                refreshIntervalSeconds: isArmServer
                    ? ClientSettings.iniConfigValue("resourcePreviewRefreshIntervalArm")
                    : ClientSettings.iniConfigValue("resourcePreviewRefreshInterval")
            }

            Row
            {
                id: contentRow

                height: delegateItem.height
                spacing: 4

                Image
                {
                    id: icon

                    width: kIconWidth
                    height: parent.height
                    fillMode: Image.Stretch
                    source: iconSource

                    Row
                    {
                        id: extras

                        spacing: 0
                        height: parent.height
                        x: -(width + (resourceTreeModel.hasChildren(modelIndex) ? 20 : 0))

                        readonly property int flags: (model && model.cameraExtraStatus) || 0

                        Image
                        {
                            id: problemsIcon
                            visible: extras.flags & ResourceTree.CameraExtraStatusFlag.buggy
                            source: "qrc:///skin/tree/buggy.png"
                        }

                        Image
                        {
                            id: recordingIcon

                            source:
                            {
                                if (extras.flags & ResourceTree.CameraExtraStatusFlag.recording)
                                    return "qrc:///skin/tree/recording.png"

                                if (extras.flags & ResourceTree.CameraExtraStatusFlag.scheduled)
                                    return "qrc:///skin/tree/scheduled.png"

                                if (extras.flags & ResourceTree.CameraExtraStatusFlag.hasArchive)
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
                    color: mainTextColor
                }

                Text
                {
                    id: extraInfo

                    text: ((resourceTreeModel.extraInfoRequired
                        || resourceTreeModel.isExtraInfoForced(delegateItem.resource)
                        || (model && model.nodeType == ResourceTree.NodeType.cloudSystem))
                            && model && model.extraInfo) || ""

                    textFormat: Text.PlainText
                    font.weight: Font.Normal
                    height: parent.height
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
                    width: delegateItem.width - x
                    verticalAlignment: Text.AlignVCenter
                    selectionColor: selectionHighlightColor
                    selectByMouse: true
                    selectedTextColor: mainTextColor
                    color: mainTextColor
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

            readonly property int kIconWidth: 20

            readonly property string iconSource:
            {
                if (!model)
                    return ""

                if (model.decorationPath)
                    return "qrc:/skin/" + model.decorationPath

                return (model.iconKey && model.iconKey !== 0
                    && ("image://resource/" + model.iconKey + "/" + itemState)) || ""
            }

            readonly property color mainTextColor:
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

            readonly property int itemState:
            {
                if (!scene || !model)
                    return ResourceTree.ItemState.normal

                switch (nodeType)
                {
                    case ResourceTree.NodeType.currentSystem:
                    case ResourceTree.NodeType.currentUser:
                        return ResourceTree.ItemState.selected

                    case ResourceTree.NodeType.layoutItem:
                    {
                        var itemLayout = modelDataAccessor.getData(
                            resourceTreeModel.parent(modelIndex), "resource")

                        if (!itemLayout || scene.currentLayout !== itemLayout)
                            return ResourceTree.ItemState.normal

                        return scene.currentResource && scene.currentResource === model.resource
                            ? ResourceTree.ItemState.accented
                            : ResourceTree.ItemState.selected
                    }

                    case ResourceTree.NodeType.resource:
                    case ResourceTree.NodeType.sharedLayout:
                    case ResourceTree.NodeType.edge:
                    case ResourceTree.NodeType.sharedResource:
                    {
                        var resource = model.resource
                        if (!resource)
                            return ResourceTree.ItemState.normal

                        if (scene.reviewedVideoWall === resource
                            || scene.controlledVideoWall === resource)
                        {
                            return ResourceTree.ItemState.selected
                        }

                        if (scene.currentResource === resource)
                            return ResourceTree.ItemState.accented

                        if (scene.currentLayout === resource
                            || scene.resources.containsResource(resource))
                        {
                            return ResourceTree.ItemState.selected
                        }

                        return ResourceTree.ItemState.normal
                    }

                    case ResourceTree.NodeType.recorder:
                    {
                        var childCount = resourceTreeModel.rowCount(modelIndex)
                        var hasSelectedChildren = false

                        for (var i = 0; i < childCount; ++i)
                        {
                            var childResource = modelDataAccessor.getData(
                                resourceTreeModel.index(i, 0, modelIndex), "resource")

                            if (!childResource)
                                continue

                            if (scene.currentResource === childResource)
                                return ResourceTree.ItemState.accented

                            hasSelectedChildren |= scene.resources.containsResource(childResource)
                        }

                        return hasSelectedChildren
                            ? ResourceTree.ItemState.selected
                            : ResourceTree.ItemState.normal
                    }

                    case ResourceTree.NodeType.videoWallItem:
                    {
                        if (!scene.controlledVideoWallItemId.isNull())
                        {
                            return scene.controlledVideoWallItemId == model.uuid
                                ? ResourceTree.ItemState.selected
                                : ResourceTree.ItemState.normal
                        }

                        const videoWall = modelDataAccessor.getData(modelIndex.parent, "resource")
                        return videoWall && videoWall === scene.reviewedVideoWall
                            ? ResourceTree.ItemState.selected
                            : ResourceTree.ItemState.normal
                    }

                    case ResourceTree.NodeType.layoutTour:
                    {
                        return scene.reviewedShowreelId == model.uuid
                            ? ResourceTree.ItemState.selected
                            : ResourceTree.ItemState.normal
                    }
                }

                return ResourceTree.ItemState.normal
            }
        }
    }

    footer: Component
    {
        Column
        {
            id: shortcutHintColumn

            width: parent.width
            topPadding: 8
            bottomPadding: 8
            leftPadding: 2
            spacing: 8
            visible: resourceTree.shortcutHintsEnabled

            Repeater
            {
                model: resourceTreeModel.shortcutHints

                ShortcutHint
                {
                    keys: modelData.keys
                    description: modelData.description
                }
            }
        }
    }

    ModelDataAccessor
    {
        id: modelDataAccessor
        model: resourceTreeModel
    }

    ResourceToolTip
    {
        id: toolTip

        Connections
        {
            target: resourceTree

            function onActivated() { toolTip.suppress() }
            function onSelectionChanged() { toolTip.suppress() }
            function onIsEditingChanged() { toolTip.suppress() }
            function onContentYChanged() { toolTip.suppress(/*immediately*/ true) }
        }

        readonly property var hoverData: {
            "item": resourceTree.hoveredItem,
            "modelIndex": resourceTree.hoveredIndex}

        onHoverDataChanged:
            updateToolTip()

        function updateToolTip()
        {
            if (!hoverData.item || hoverData.item.isEditing || hoverData.item.isSeparator)
            {
                hide()
                return
            }

            target = hoverData.item
            thumbnailSource = hoverData.item.thumbnailSource

            enclosingRect = hoverData.item.mapFromItem(resourceTree,
                resourceTree.toolTipEnclosingRect)

            var text = modelDataAccessor.getData(hoverData.modelIndex, "display")
            var extra = modelDataAccessor.getData(hoverData.modelIndex, "extraInfo")

            if (text && extra)
                text = `<b>${text}</b> ${extra}`

            toolTip.text = text || ""

            if (state !== BubbleToolTip.Suppressed)
                show()
        }
    }
}
