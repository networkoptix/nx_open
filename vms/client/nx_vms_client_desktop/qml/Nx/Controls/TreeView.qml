// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15
import QtQml.Models 2.15

import Nx 1.0
import Nx.Controls 1.0

import nx.client.desktop 1.0
import nx.vms.client.core 1.0
import nx.vms.client.desktop 1.0

/**
 * A tree view with multi-selection, drag-n-drop and editing support.
 */
FocusScope
{
    id: treeView

    property AbstractItemModel model: null
    property alias rootIndex: linearizationListModel.sourceRoot
    property Component delegate: null
    property alias autoExpandRoleName: linearizationListModel.autoExpandRoleName

    property real indentation: 24 //< Horizontal indent of next tree level, in pixels.
    property bool rootIsDecorated: true
    property bool keyNavigationEnabled: true
    property alias itemHeightHint: listView.itemHeightHint
    property alias scrollStepSize: listView.scrollStepSize //< In pixels.

    property alias hoverHighlightColor: listView.hoverHighlightColor
    property color selectionHighlightColor: "teal"
    property color dropHighlightColor: "blue"

    readonly property bool empty: !listView.count

    readonly property alias currentIndex: currentIndexWatcher.currentSourceIndex

    readonly property alias currentRowItem: listView.currentItem
    readonly property Item currentItem: currentRowItem && currentRowItem.delegateItem

    readonly property var hoveredIndex: listView.hoveredItem
        ? listView.hoveredItem.sourceIndex
        : NxGlobals.invalidModelIndex()

    readonly property alias hoveredRowItem: listView.hoveredItem
    readonly property Item hoveredItem: hoveredRowItem && hoveredRowItem.delegateItem

    property real leftMargin: 0
    property real rightMargin: 0
    property alias topMargin: listView.topMargin
    property alias bottomMargin: listView.bottomMargin

    property alias highlight: listView.highlight

    property int autoExpandDelayMs:
        Utils.getValue(ClientSettings.iniConfigValue("autoExpandDelayMs"), -1)

    property int maximumDragIndicatorLines: 10
    property real maximumDragIndicatorWidth: 640

    property var expandsOnDoubleClick: (modelIndex => true) //< May be a bool value or a functor.
    property var activateOnSingleClick: (modelIndex => false)

    property alias header: listView.header
    property alias headerPositioning: listView.headerPositioning
    readonly property alias headerItem: listView.headerItem

    property alias footer: listView.footer
    property alias footerPositioning: listView.footerPositioning
    readonly property alias footerItem: listView.footerItem
    readonly property alias contentX: listView.contentX
    readonly property alias contentY: listView.contentY

    clip: true

    implicitWidth: listView.implicitWidth
    implicitHeight: listView.implicitHeight

    function expandAll() { linearizationListModel.expandAll() }
    function collapseAll() { linearizationListModel.collapseAll() }
    function expandedState() { return linearizationListModel.expandedState() }
    function setExpandedState(state) { linearizationListModel.setExpandedState(state) }
    function expand(shouldExpand, topLevelRows)
    {
        linearizationListModel.setExpanded(shouldExpand, topLevelRows)
    }

    function startEditing()
    {
        if (listView.currentItem)
            listView.currentItem.tryStartEditing()
    }

    function selection()
    {
        if (selectionModel.cachedSourceSelection === undefined)
        {
            selectionModel.cachedSourceSelection = selectionModel.optimizedSelectedIndexes.map(
                index => NxGlobals.toPersistent(linearizationListModel.mapToSource(index)))
        }

        return selectionModel.cachedSourceSelection
    }

    /** Clear selection, but keep current index. */
    function clearSelection()
    {
        selectionModel.clearSelection()
    }

    function setSelection(indexes)
    {
        ensureVisible(indexes)
        selectionModel.clear()

        const linearIndexes = indexes.map(linearizationListModel.mapFromSource)
            .filter(index => index.valid)

        if (!linearIndexes.length)
            return

        selectionModel.setCurrentIndex(linearIndexes[0], ItemSelectionModel.Select)

        for (let i = 1; i < linearIndexes.length; ++i)
            selectionModel.select(linearIndexes[i], ItemSelectionModel.Select)
    }

    function ensureVisible(indexes)
    {
        linearizationListModel.ensureVisible(indexes)
    }

    function isExpanded(index)
    {
        return linearizationListModel.isSourceExpanded(index)
    }

    function setExpanded(index, value)
    {
        linearizationListModel.setSourceExpanded(index, value)
    }

    function setCurrentIndex(modelIndex, selectionFlags)
    {
        selectionModel.setCurrentIndex(linearizationListModel.mapFromSource(modelIndex),
            selectionFlags ?? ItemSelectionModel.ClearAndSelect)
    }

    function rowItemAtIndex(modelIndex)
    {
        const linearIndex = linearizationListModel.mapFromSource(modelIndex)
        return linearIndex.valid ? listView.itemAtIndex(linearIndex.row) : null
    }

    function itemAtIndex(modelIndex)
    {
        const rowItem = rowItemAtIndex(modelIndex)
        return rowItem && rowItem.delegateItem
    }

    signal activated(var modelIndex, var selection, var activationType, var modifiers)
    signal contextMenuRequested(point globalPos, var modelIndex, var selection)
    signal selectionChanged()

    ListView
    {
        id: listView

        anchors.fill: parent
        model: linearizationListModel
        currentIndex: currentIndexWatcher.index.row
        keyNavigationEnabled: false //< We implement our own key nativation.
        highlightFollowsCurrentItem: false //< We implement our own scroll to current item.
        reuseItems: true

        // Approximate items per page for key navigation.
        readonly property int itemsPerPage: treeView.itemHeightHint
            ? height / treeView.itemHeightHint
            : height / 20 //< Some default.

        readonly property real kExpandCollapseButtonWidth: 24

        property bool justGotFocus: false

        LinearizationListModel
        {
            id: linearizationListModel
            sourceModel: treeView.model

            function canExpand(index)
            {
                return data(index, LinearizationListModel.HasChildrenRole)
                    && !data(index, LinearizationListModel.ExpandedRole)
            }
        }

        PersistentIndexWatcher
        {
            id: currentIndexWatcher

            index: selectionModel.currentIndex

            readonly property var currentSourceIndex:
                NxGlobals.toPersistent(linearizationListModel.mapToSource(index))

            property var previousSourceIndex

            onCurrentSourceIndexChanged:
            {
                if (currentSourceIndex === previousSourceIndex)
                    return

                previousSourceIndex = currentSourceIndex

                // Immediate repositioning instead of animated flick.
                listView.positionViewAtIndex(index.row, ListView.Contain)
            }
        }

        ItemSelectionModel
        {
            id: selectionModel
            model: linearizationListModel

            property var cachedSourceSelection: undefined

            readonly property var optimizedSelectedIndexes: Array.prototype.map.call(
                // Convert selectedIndexes to QVariantList to avoid huge performance loss.
                NxGlobals.toVariantList(selectedIndexes),
                NxGlobals.toPersistent)

            onOptimizedSelectedIndexesChanged:
            {
                cachedSourceSelection = undefined
                treeView.selectionChanged()
            }

            function hasDraggableIndexes()
            {
                return optimizedSelectedIndexes.some(
                    (index) => linearizationListModel.flags(index) & Qt.ItemIsDragEnabled)
            }
        }

        ListNavigationHelper
        {
            id: navigationHelper

            selectionModel: selectionModel
            pageSize: listView.itemsPerPage
        }

        Timer
        {
            id: autoExpandTimer

            property var modelIndex: NxGlobals.invalidModelIndex()
            interval: treeView.autoExpandDelayMs
            repeat: false

            function update(newModelIndex)
            {
                if (newModelIndex == modelIndex)
                    return

                modelIndex = NxGlobals.toPersistent(newModelIndex)

                if (modelIndex.valid)
                    restart()
                else
                    stop()
            }

            onTriggered:
            {
                if (!modelIndex.valid)
                    return

                linearizationListModel.setData(
                    modelIndex, true, LinearizationListModel.ExpandedRole)
            }
        }

        Timer
        {
            id: editingTimer

            property var item: null
            interval: Qt.styleHints.mouseDoubleClickInterval
            repeat: false

            onTriggered:
            {
                if (!item)
                    return

                item.startEditing()
                item = null
            }
        }

        Connections
        {
            target: editingTimer.item
            ignoreUnknownSignals: !editingTimer.item

            function onIsCurrentChanged()
            {
                editingTimer.item = null
                editingTimer.stop()
            }
        }

        delegate: Component
        {
            Item
            {
                id: listItem

                width: parent ? parent.width : 0
                implicitHeight: Math.max(button.implicitHeight, delegateLoader.implicitHeight)
                clip: true
                enabled: itemFlags & Qt.ItemIsEnabled
                opacity: enabled ? 1.0 : 0.3
                visible: inUse

                readonly property bool isSelectable: itemFlags & Qt.ItemIsSelectable
                readonly property bool isEditable: itemFlags & Qt.ItemIsEditable

                readonly property var modelData: model

                readonly property var modelIndex: inUse
                    ? linearizationListModel.index(index, 0)
                    : NxGlobals.invalidModelIndex()

                readonly property var sourceIndex: NxGlobals.toPersistent(
                    linearizationListModel.mapToSource(modelIndex))

                readonly property int itemFlags: inUse ? itemFlagsWatcher.itemFlags : 0
                readonly property bool isCurrent: ListView.isCurrentItem
                readonly property alias isSelected: selectionHighlight.isItemSelected

                readonly property alias delegateItem: delegateLoader.item

                readonly property bool isDelegateFocused:
                    delegateItem ? delegateItem.activeFocus : false

                property bool inUse: true
                ListView.onPooled: inUse = false
                ListView.onReused: inUse = true

                function tryStartEditing()
                {
                    if (isCurrent && isEditable)
                        delegateLoader.startEditing()
                }

                ContextHelp.topicId: (model && model.helpTopicId) || 0

                ModelItemFlagsWatcher
                {
                    id: itemFlagsWatcher
                    index: listItem.modelIndex
                }

                Rectangle
                {
                    id: selectionHighlight

                    property bool isItemSelected:
                    {
                        if (!listItem.inUse)
                            return false

                        return /*re-evaluate on change*/ selectionModel.selection,
                            selectionModel.isRowSelected(index, NxGlobals.invalidModelIndex())
                    }

                    anchors.fill: parent
                    visible: isItemSelected

                    color: treeView.activeFocus && !isDelegateFocused
                        ? selectionHighlightColor
                        : hoverHighlightColor
                }

                DropArea
                {
                    id: dropArea

                    anchors.fill: parent
                    visible: listItem.inUse

                    onEntered:
                    {
                        drag.accepted = (itemFlags & Qt.ItemIsDropEnabled)
                            && (checkDrop(drag, drag.proposedAction)
                                || checkDrop(drag, Qt.MoveAction)
                                || checkDrop(drag, Qt.CopyAction)
                                || checkDrop(drag, Qt.LinkAction))
                    }

                    onDropped:
                    {
                        drop.accepted = (itemFlags & Qt.ItemIsDropEnabled)
                            && DragAndDrop.drop(currentMimeData, drop.action, modelIndex)
                    }

                    function canDrop(drag, action)
                    {
                        return (drag.supportedActions & action)
                            && (DragAndDrop.supportedDropActions(linearizationListModel) & action)
                            && DragAndDrop.canDrop(currentMimeData, action, modelIndex)
                    }

                    function checkDrop(drag, action)
                    {
                        if (!canDrop(drag, action))
                            return false

                        drag.action = action
                        return true
                    }

                    Rectangle
                    {
                        id: dropHighlight

                        anchors.fill: parent
                        color: "transparent"
                        border.color: treeView.dropHighlightColor
                        border.width: 1
                        radius: 2
                        visible: dropArea.containsDrag
                    }
                }

                Image
                {
                    id: button

                    width: listView.kExpandCollapseButtonWidth
                    x: treeView.leftMargin + model.level * indentation
                        - (rootIsDecorated ? 0 : width)

                    height: listItem.height
                    fillMode: Image.Pad
                    verticalAlignment: Image.AlignVCenter
                    visible: rootIsDecorated || model.level

                    source:
                    {
                        if (!model.hasChildren)
                            return ""

                        return model.expanded
                            ? "qrc:/skin/tree/branch_open.png"
                            : "qrc:/skin/tree/branch_closed.png"
                    }
                }

                MouseArea
                {
                    id: mouseArea

                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton
                    visible: listItem.inUse

                    drag.onActiveChanged:
                    {
                        if (drag.active && selectionModel.hasDraggableIndexes())
                        {
                            dragIndicator.active = true
                            reselectOnRelease = false

                            ItemGrabber.grabToImage(dragIndicator,
                                function (resultUrl)
                                {
                                    dragIndicator.active = false
                                    if (!mouseArea.drag.active)
                                        return

                                    DragAndDrop.execute(
                                        treeView,
                                        DragAndDrop.createMimeData(
                                            selectionModel.optimizedSelectedIndexes),
                                        DragAndDrop.supportedDragActions(linearizationListModel),
                                        Qt.MoveAction,
                                        resultUrl)
                                })
                        }
                    }

                    onDoubleClicked:
                    {
                        editingTimer.stop()
                        reselectOnRelease = false

                        const toggleable = model && model.hasChildren
                        if (toggleable && button.contains(mapToItem(button, mouse.x, mouse.y)))
                            return

                        const toggle = toggleable
                            && ((typeof treeView.expandsOnDoubleClick === "function")
                                    ? treeView.expandsOnDoubleClick(listItem.sourceIndex)
                                    : !!treeView.expandsOnDoubleClick)
                        if (toggle)
                        {
                            linearizationListModel.setExpanded(modelIndex, !model.expanded,
                                /*recursively*/ mouse.modifiers & Qt.AltModifier)
                        }
                        else
                        {
                            activated(listItem.sourceIndex, selection(),
                                ResourceTree.ActivationType.doubleClick,
                                Qt.NoModifier)
                        }
                    }

                    onPressed:
                    {
                        listView.justGotFocus = !treeView.focus
                        treeView.focus = true

                        const toggle = model && model.hasChildren
                            && button.contains(mapToItem(button, mouse.x, mouse.y))

                        if (toggle)
                        {
                            linearizationListModel.setExpanded(modelIndex, !model.expanded,
                                /*recursively*/ mouse.modifiers & Qt.AltModifier)
                            return
                        }

                        drag.target = (mouse.button == Qt.LeftButton)
                            ? dragIndicator
                            : null

                        if (isSelectable)
                        {
                            if (!modelIndex.valid)
                                return

                            if (mouse.button == Qt.MiddleButton || treeView.activateOnSingleClick(listItem.sourceIndex))
                            {
                                activated(modelIndex,
                                    selection(),
                                    (mouse.button == Qt.MiddleButton)
                                        ? ResourceTree.ActivationType.middleClick
                                        : ResourceTree.ActivationType.singleClick,
                                    Qt.NoModifier);
                            }
                            else
                            {
                                if (isSelected
                                        && !(mouse.modifiers & (Qt.ControlModifier | Qt.ShiftModifier)))
                                {
                                    if (isDelegateFocused)
                                        forceActiveFocus()
                                    else
                                        reselectOnRelease = true
                                }
                                else
                                {
                                    navigationHelper.navigateTo(modelIndex, mouse.modifiers)
                                }
                            }
                        }
                    }

                    onReleased:
                    {
                        const justGotFocus = listView.justGotFocus
                        listView.justGotFocus = false

                        if (mouse.button == Qt.RightButton)
                        {
                            contextMenuRequested(
                                mouseArea.mapToGlobal(mouse.x, mouse.y),
                                listItem.sourceIndex, //< Right-clicked index.
                                treeView.selection())

                            reselectOnRelease = false
                            return
                        }

                        if (!reselectOnRelease)
                            return

                        reselectOnRelease = false

                        if (modelIndex.valid && containsMouse)
                        {
                            const editingRequired = isEditable
                                && !justGotFocus
                                && mouse.button == Qt.LeftButton
                                && selectionModel.selection.length == 1
                                && selectionModel.selection[0].height == 1

                            selectionModel.setCurrentIndex(
                                modelIndex, ItemSelectionModel.ClearAndSelect)

                            if (editingRequired)
                            {
                                editingTimer.item = delegateLoader
                                editingTimer.restart()
                            }
                        }
                    }

                    property bool reselectOnRelease: false
                }

                Loader
                {
                    id: delegateLoader

                    readonly property var model: listItem.modelData
                    readonly property var modelIndex: listItem.sourceIndex
                    readonly property int itemFlags: listItem.itemFlags

                    readonly property bool isCurrent: listItem.isCurrent
                    readonly property bool isSelected: listItem.isSelected

                    readonly property real itemIndent: x

                    signal startEditing()
                    signal finishEditing()

                    sourceComponent: treeView.delegate
                    visible: listItem.inUse

                    anchors.left: button.right
                    anchors.right: listItem.right
                    anchors.rightMargin: treeView.rightMargin
                        + (listView.ScrollBar.vertical.visible ? listView.ScrollBar.vertical.width : 0)

                    Connections
                    {
                        target: selectionModel
                        enabled: delegateLoader.isCurrent

                        function onSelectionChanged() { delegateLoader.finishEditing() }
                        function onCurrentChanged() { delegateLoader.finishEditing() }
                    }
                }
            }
        }

        DragHoverWatcher
        {
            target: listView

            onPositionChanged:
            {
                autoScroller.velocity = ProximityScrollHelper.velocity(
                    Qt.rect(0, 0, listView.width, listView.height),
                    position)

                if (treeView.autoExpandDelayMs < 0)
                    return

                const item = listView.itemAt(
                    listView.contentX + position.x,
                    listView.contentY + position.y)

                autoExpandTimer.update(
                    (item && item.modelIndex && linearizationListModel.canExpand(item.modelIndex))
                        ? item.modelIndex
                        : NxGlobals.invalidModelIndex())
            }

            onStopped:
            {
                autoExpandTimer.update(NxGlobals.invalidModelIndex())
                autoScroller.velocity = 0
            }
        }

        AutoScroller
        {
            id: autoScroller

            scrollBar: listView.scrollBar
            contentSize: listView.contentHeight
        }

        FocusFrame
        {
            id: currentItemHighlight

            color: ColorTheme.transparent(ColorTheme.highlight, 0.5)
            z: 1

            visible: treeView.activeFocus
                && listView.currentItem
                && listView.currentItem.isSelectable
                && !listView.currentItem.isSelected

            parent: listView.contentItem
            anchors.fill: listView.currentItem || undefined
        }

        Rectangle
        {
            id: dragIndicator

            property bool active: false

            width: Math.max(dragIndicatorColumn.width, 1)
            height: Math.max(dragIndicatorColumn.height, 1)
            color: ColorTheme.transparent("black", 0.8)
            visible: false

            Rectangle
            {
                anchors.fill: parent
                color: selectionHighlightColor
            }

            IndexListModel
            {
                id: indexListModel

                readonly property int remainder:
                    selectionModel.optimizedSelectedIndexes.length
                        - treeView.maximumDragIndicatorLines

                source: Array.prototype.slice.call(selectionModel.optimizedSelectedIndexes,
                    0, Math.min(selectionModel.optimizedSelectedIndexes.length,
                        treeView.maximumDragIndicatorLines))
            }

            Column
            {
                id: dragIndicatorColumn

                Repeater
                {
                    model: dragIndicator.active ? indexListModel : []

                    delegate: Component
                    {
                        Item
                        {
                            id: draggedItem

                            readonly property var modelData: model
                            readonly property var modelIndex: indexListModel.sourceIndex(index)

                            readonly property int itemFlags:
                                linearizationListModel.flags(modelIndex)

                            readonly property int kRightMargin: 12

                            width: dragDelegateLoader.x + dragDelegateLoader.width + kRightMargin
                            height: dragDelegateLoader.height
                            visible: itemFlags & Qt.ItemIsDragEnabled

                            Loader
                            {
                                id: dragDelegateLoader

                                readonly property var model: draggedItem.modelData

                                readonly property var modelIndex: NxGlobals.toPersistent(
                                    linearizationListModel.mapToSource(draggedItem.modelIndex))

                                readonly property int itemFlags: draggedItem.itemFlags
                                readonly property bool isCurrent: false
                                readonly property bool isSelected: true
                                readonly property real itemIndent: x

                                sourceComponent: treeView.delegate
                                x: listView.kExpandCollapseButtonWidth

                                width: Math.min(implicitWidth, treeView.maximumDragIndicatorWidth
                                    - x - draggedItem.kRightMargin)

                                // Signals for compatibility with the underlying delegate.
                                signal startEditing()
                                signal finishEditing()
                            }
                        }
                    }
                }

                Text
                {
                    id: remainderText

                    text: qsTr("... and %n more", "", indexListModel.remainder)
                    color: ColorTheme.colors.light10
                    leftPadding: 16
                    topPadding: 4
                    clip: true

                    // We shouldn't touch `visible` here to avoid troubles with pixmap grabbing.
                    height: (indexListModel.remainder > 0) ? 24 : 0
                }
            }
        }
    }

    Keys.onPressed:
    {
        if (!listView.count)
            return

        event.accepted = true
        switch (event.key)
        {
            case Qt.Key_Enter:
            case Qt.Key_Return:
                if (treeView.currentIndex.valid)
                    activated(treeView.currentIndex,
                        selection(),
                        ResourceTree.ActivationType.enterKey,
                        event.modifiers)
                break

            case Qt.Key_Space:
                selectionModel.select(currentIndexWatcher.index,
                    (event.modifiers & Qt.ControlModifier)
                        ? ItemSelectionModel.Toggle
                        : ItemSelectionModel.Select)
                break

            case Qt.Key_F2:
                if (selectionModel.optimizedSelectedIndexes.length == 1)
                    treeView.startEditing()
                break

            case Qt.Key_Right:
            case Qt.Key_Plus:
                linearizationListModel.setExpanded(currentIndexWatcher.index, true,
                    /*recursively*/ event.modifiers & Qt.AltModifier)
                break

            case Qt.Key_Left:
            case Qt.Key_Minus:
                linearizationListModel.setExpanded(currentIndexWatcher.index, false,
                    /*recursively*/ event.modifiers & Qt.AltModifier)
                break

            default:
                event.accepted = false
                break
        }

        if (event.accepted || !treeView.keyNavigationEnabled)
            return

        event.accepted = true
        switch (event.key)
        {
            case Qt.Key_Up:
                navigationHelper.navigate(ListNavigationHelper.Move.up, event.modifiers)
                break

            case Qt.Key_Down:
                navigationHelper.navigate(ListNavigationHelper.Move.down, event.modifiers)
                break

            case Qt.Key_PageUp:
                navigationHelper.navigate(ListNavigationHelper.Move.pageUp, event.modifiers)
                break

            case Qt.Key_PageDown:
                navigationHelper.navigate(ListNavigationHelper.Move.pageDown, event.modifiers)
                break

            case Qt.Key_Home:
                navigationHelper.navigate(ListNavigationHelper.Move.home, event.modifiers)
                break

            case Qt.Key_End:
                navigationHelper.navigate(ListNavigationHelper.Move.end, event.modifiers)
                break

            default:
                event.accepted = false
                break
        }
    }
}
