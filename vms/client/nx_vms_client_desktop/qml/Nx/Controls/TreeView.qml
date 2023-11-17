// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQml.Models

import Nx
import Nx.Core
import Nx.Controls

import nx.client.desktop
import nx.vms.client.core
import nx.vms.client.desktop

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
    property bool selectionEnabled: true
    property alias itemHeightHint: listView.itemHeightHint
    property alias scrollStepSize: listView.scrollStepSize //< In pixels.
    property alias spacing: listView.spacing

    // If this data role is defined, Ctrl+A shortcut selects all siblings of the current index
    // that share the same value of this data role.
    property string selectAllSiblingsRoleName

    // Whether the selection overlaps spacing between items. As in that case neighboring selection
    // lines overlap each other, `selectionHighlightColor` must be opaque.
    property bool selectionOverlapsSpacing: false

    property alias hoverHighlightColor: listView.hoverHighlightColor
    property color selectionHighlightColor: "teal"
    property color inactiveSelectionHighlightColor: hoverHighlightColor
    property color dropHighlightColor: "blue"

    property bool editable: true

    readonly property bool empty: !listView.count

    readonly property alias currentIndex: currentIndexWatcher.currentSourceIndex

    readonly property alias currentRowItem: listView.currentItem
    readonly property Item currentItem: currentRowItem && currentRowItem.delegateItem

    readonly property var hoveredIndex: listView.hoveredItem
        ? listView.hoveredItem.sourceIndex
        : NxGlobals.invalidModelIndex()

    readonly property alias hoveredRowItem: listView.hoveredItem
    readonly property Item hoveredItem: hoveredRowItem && hoveredRowItem.delegateItem

    readonly property bool hasSelection: selectionModel.effectiveSelectedIndexes.length > 0

    // Whether clicking on a single selected item unselects it.
    property bool clickUnselectsSingleSelectedItem: false

    property real leftMargin: 0
    property real rightMargin: 0
    property alias topMargin: listView.topMargin
    property alias bottomMargin: listView.bottomMargin

    property alias visibleArea: listView.visibleArea

    property alias scrollBar: listView.scrollBar
    readonly property bool scrollBarVisible: visibleArea.heightRatio < 1.0

    property alias highlight: listView.highlight

    property int autoExpandDelayMs:
        Utils.getValue(LocalSettings.iniConfigValue("autoExpandDelayMs"), -1)

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

    readonly property alias contentItem: listView.contentItem
    readonly property alias contentX: listView.contentX
    readonly property alias contentY: listView.contentY

    readonly property alias autoScroller: autoScroller

    property bool clipDelegates: true

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
            selectionModel.cachedSourceSelection = selectionModel.effectiveSelectedIndexes.map(
                index => NxGlobals.toPersistent(linearizationListModel.mapToSource(index)))
        }

        return selectionModel.cachedSourceSelection
    }

    function clearSelection(clearCurrentIndex)
    {
        selectionModel.clearSelection()

        if (clearCurrentIndex)
            selectionModel.clearCurrentIndex()
    }

    function setSelection(indexes)
    {
        ensureVisible(indexes)
        selectionModel.clear()

        const linearIndexes = indexes.map((source) => linearizationListModel.mapFromSource(source))
            .filter(index => index.valid)

        if (!linearIndexes.length)
            return

        selectionModel.setCurrentIndex(linearIndexes[0], ItemSelectionModel.Select)

        for (let i = 1; i < linearIndexes.length; ++i)
            selectionModel.select(linearIndexes[i], ItemSelectionModel.Select)
    }

    /**
     * Changes selection to all siblings of the current index that share with it
     * the same value of the specified `roleName`.
     */
    function selectAllSiblings(roleName)
    {
        if (!currentIndex.valid)
            return

        setSelection(NxGlobals.modelFindAll(
            currentIndex,
            roleName,
            NxGlobals.modelData(currentIndex, roleName),
            Qt.MatchExactly | Qt.MatchWrap))
    }

    function isSelected(index)
    {
        const linearIndex = linearizationListModel.mapFromSource(index)
        return linearIndex.valid && selectionModel.isSelected(linearIndex)
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

    function rowIndexes(firstRow, lastRow)
    {
        let result = []
        for (let row = firstRow; row <= lastRow; ++row)
            result.push(linearizationListModel.mapToSource(linearizationListModel.index(row, 0)))

        return result
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

            onModelAboutToBeReset:
                selectionModel.clear() //< QItemSelectionModel doesn't do it on its own.
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
            property var effectiveSelectedIndexes: []

            function updateSelectedIndexes()
            {
                effectiveSelectedIndexes = Array.prototype.filter.call(
                    // Convert selectedIndexes to QVariantList to avoid huge performance loss.
                    NxGlobals.toQVariantList(selectedIndexes), (index) => index.valid)
                        .map(index => NxGlobals.toPersistent(index))

                cachedSourceSelection = undefined
                treeView.selectionChanged()
            }

            function hasDraggableIndexes()
            {
                return effectiveSelectedIndexes.some(
                    (index) => linearizationListModel.flags(index) & Qt.ItemIsDragEnabled)
            }

            onSelectionChanged:
                updateSelectedIndexes()
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
                enabled: inUse && (itemFlags & Qt.ItemIsEnabled)
                clip: treeView.clipDelegates
                opacity: enabled ? 1.0 : 0.3
                visible: inUse

                readonly property bool isSelectable: treeView.selectionEnabled
                    && (itemFlags & Qt.ItemIsSelectable)

                readonly property bool isEditable: treeView.editable
                    && (itemFlags & Qt.ItemIsEditable)

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

                containmentMask: QtObject
                {
                    function contains(point: point): bool
                    {
                        const extra = treeView.selectionOverlapsSpacing ? treeView.spacing : 0

                        return point.x >= 0 && point.y >= 0 && point.x < listItem.width
                            && point.y < listItem.height + extra
                    }
                }

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

                    property int margin: treeView.selectionOverlapsSpacing ? -treeView.spacing : 0

                    parent: listItem.parent
                    visible: isItemSelected
                    z: -10

                    anchors.fill: (parent === listItem.parent) ? listItem : undefined
                    anchors.topMargin: margin
                    anchors.bottomMargin: margin

                    color: treeView.activeFocus && !listItem.isDelegateFocused
                        ? treeView.selectionHighlightColor
                        : treeView.inactiveSelectionHighlightColor
                }

                DropArea
                {
                    id: dropArea

                    anchors.fill: parent
                    visible: listItem.inUse

                    onEntered: (drag) =>
                    {
                        drag.accepted = (itemFlags & Qt.ItemIsDropEnabled)
                            && (checkDrop(drag, drag.proposedAction)
                                || checkDrop(drag, Qt.MoveAction)
                                || checkDrop(drag, Qt.CopyAction)
                                || checkDrop(drag, Qt.LinkAction))
                    }

                    onDropped: (drop) =>
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
                            ? "image://svg/skin/tree/arrow_open_20.svg"
                            : "image://svg/skin/tree/arrow_close_20.svg"
                    }
                    sourceSize: Qt.size(20, 20)
                }

                MouseArea
                {
                    id: mouseArea

                    anchors.fill: parent
                    anchors.bottomMargin: treeView.selectionOverlapsSpacing ? -treeView.spacing : 0

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
                                        listItem,
                                        DragAndDrop.createMimeData(
                                            selectionModel.effectiveSelectedIndexes),
                                        DragAndDrop.supportedDragActions(linearizationListModel),
                                        Qt.MoveAction,
                                        resultUrl)
                                })
                        }
                    }

                    onDoubleClicked: (mouse) =>
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

                    onPressed: (mouse) =>
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

                            if (mouse.button == Qt.MiddleButton
                                || treeView.activateOnSingleClick(listItem.sourceIndex))
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

                    onReleased: (mouse) =>
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

                        if (!modelIndex.valid || !containsMouse)
                            return

                        const editingRequired = isEditable
                            && !justGotFocus
                            && mouse.button == Qt.LeftButton
                            && selectionModel.selection.length == 1
                            && selectionModel.selection[0].height == 1

                        if (treeView.clickUnselectsSingleSelectedItem
                            && selectionModel.effectiveSelectedIndexes.length === 1
                            && NxGlobals.fromPersistent(
                                selectionModel.effectiveSelectedIndexes[0]) == modelIndex)
                        {
                            selectionModel.clearSelection()
                            selectionModel.clearCurrentIndex()
                        }
                        else
                        {
                            selectionModel.setCurrentIndex(
                                modelIndex, ItemSelectionModel.ClearAndSelect)
                        }

                        if (editingRequired)
                        {
                            editingTimer.item = delegateLoader
                            editingTimer.restart()
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

                    readonly property Item rowItem: listItem
                    readonly property Item selectionItem: selectionHighlight

                    readonly property real itemIndent: x

                    signal startEditing()
                    signal finishEditing()

                    sourceComponent: treeView.delegate
                    visible: listItem.inUse

                    anchors.left: button.right
                    anchors.right: listItem.right

                    anchors.rightMargin: treeView.rightMargin
                        + (listView.ScrollBar.vertical.visible
                            ? listView.ScrollBar.vertical.width
                            : 0)

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

            onPositionChanged: (position) =>
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
            z: 10 //< Above everything.

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
                    selectionModel.effectiveSelectedIndexes.length
                        - treeView.maximumDragIndicatorLines

                source: Array.prototype.slice.call(selectionModel.effectiveSelectedIndexes,
                    0, Math.min(selectionModel.effectiveSelectedIndexes.length,
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

    Keys.onPressed: (event) =>
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
                if (selectionModel.effectiveSelectedIndexes.length == 1)
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

    Keys.onShortcutOverride: (event) =>
    {
        if (event.key != Qt.Key_A || event.modifiers != Qt.ControlModifier)
            return

        if (!selectionEnabled || !keyNavigationEnabled || !selectAllSiblingsRoleName)
            return

        event.accepted = true
        selectAllSiblings(selectAllSiblingsRoleName)
    }
}
