import QtQuick 2.6
import QtQml.Models 2.11

import Nx 1.0
import Nx.Controls 1.0

import nx.vms.client.desktop 1.0

/**
 * A tree view with multi-selection, drag-n-drop and editing support.
 */
Item
{
    id: treeView

    property var model: null
    property Component delegate: null
    property real indentation: 24 //< Horizontal indent of next tree level, in pixels.

    property alias scrollStepSize: listView.scrollStepSize //< In pixels.
    property alias hoverHighlightColor: listView.hoverHighlightColor
    property color selectionHighlightColor: "teal"

    ListView
    {
        id: listView

        anchors.fill: parent
        currentIndex: selectionModel.currentIndex.row
        model: linearizationListModel
        highlight: null

        LinearizationListModel
        {
            id: linearizationListModel
            sourceModel: treeView.model
        }

        ItemSelectionModel
        {
            id: selectionModel
            model: linearizationListModel
        }

        delegate: Component
        {
            Item
            {
                id: treeItem

                width: parent.width
                implicitHeight: Math.max(button.implicitHeight, delegateLoader.implicitHeight)
                clip: true

                readonly property bool isSelectable: itemFlags & Qt.ItemIsSelectable
                readonly property bool isEditable: itemFlags & Qt.ItemIsEditable

                readonly property var modelData: model
                readonly property var modelIndex: linearizationListModel.index(index, 0)
                readonly property int itemFlags: linearizationListModel.flags(modelIndex)
                readonly property bool isCurrent: ListView.isCurrentItem
                readonly property bool isSelected: selectionHighlight.visible

                readonly property bool isDelegateFocused: delegateLoader.item
                    && delegateLoader.item.activeFocus

                Rectangle
                {
                    id: selectionHighlight

                    anchors.fill: parent
                    visible: false

                    color: listView.activeFocus && !isDelegateFocused
                        ? selectionHighlightColor
                        : hoverHighlightColor

                    Connections
                    {
                        target: selectionModel

                        onSelectionChanged:
                            selectionHighlight.visible = selectionModel.isRowSelected(index, null)
                    }
                }

                DropArea
                {
                    id: dropArea

                    anchors.fill: parent

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
                        return (drag.supportedActions && action)
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
                }

                Image
                {
                    id: button

                    width: 24
                    x: model.level * indentation

                    height: parent.height
                    fillMode: Image.Pad
                    verticalAlignment: Image.AlignVCenter

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

                    drag.onActiveChanged:
                    {
                        if (drag.active && dragIndicator.height > 0)
                        {
                            reselectOnRelease = false

                            ItemGrabber.grabToImage(dragIndicator,
                                function (resultUrl)
                                {
                                    if (!mouseArea.drag.active)
                                        return

                                    DragAndDrop.execute(
                                        treeView,
                                        DragAndDrop.createMimeData(selectionModel.selectedIndexes),
                                        DragAndDrop.supportedDragActions(linearizationListModel),
                                        Qt.MoveAction,
                                        resultUrl)
                                })
                        }
                        else
                        {
                            drag.target = null
                        }
                    }

                    onDoubleClicked:
                    {
                        var toggle = model && model.hasChildren
                            && !button.contains(mapToItem(button, mouse.x, mouse.y))

                        if (toggle)
                            model.expanded = !model.expanded
                    }

                    onPressed:
                    {
                        if (mouse.flags & Qt.MouseEventCreatedDoubleClick)
                            return

                        listView.focus = true

                        var toggle = model && model.hasChildren
                            && button.contains(mapToItem(button, mouse.x, mouse.y))

                        if (toggle)
                        {
                            model.expanded = !model.expanded
                            return
                        }

                        drag.target = (mouse.button == Qt.LeftButton)
                            ? dragIndicator
                            : null

                        if (isSelectable)
                        {
                            if (!modelIndex.valid)
                                return

                            var controlPressed = mouse.modifiers & Qt.ControlModifier
                            var shiftPressed = mouse.modifiers & Qt.ShiftModifier

                            if (controlPressed)
                            {
                                selectionModel.setCurrentIndex(
                                    modelIndex, ItemSelectionModel.Toggle)
                            }
                            else if (shiftPressed && selectionModel.currentIndex.valid)
                            {
                                var selection = ItemModelUtils.createSelection(
                                    selectionModel.currentIndex, modelIndex)

                                selectionModel.select(selection, ItemSelectionModel.SelectCurrent)
                            }
                            else if (!isSelected)
                            {
                                selectionModel.setCurrentIndex(
                                    modelIndex, ItemSelectionModel.ClearAndSelect)
                            }
                            else
                            {
                                if (isDelegateFocused)
                                    forceActiveFocus()
                                else
                                    reselectOnRelease = true
                            }
                        }
                    }

                    onReleased:
                    {
                        if (!reselectOnRelease)
                            return

                        reselectOnRelease = false

                        if (modelIndex.valid && containsMouse)
                        {
                            selectionModel.setCurrentIndex(
                                modelIndex, ItemSelectionModel.ClearAndSelect)

                            if (isEditable)
                                delegateLoader.startEditing()
                        }
                    }

                    property bool reselectOnRelease: false
                }

                Loader
                {
                    id: delegateLoader

                    readonly property var model: treeItem.modelData

                    readonly property var modelIndex: linearizationListModel.mapToSource(
                        treeItem.modelIndex)

                    readonly property int itemFlags: treeItem.itemFlags

                    readonly property bool isCurrent: treeItem.isCurrent
                    readonly property bool isSelected: treeItem.isSelected

                    readonly property real itemIndent: x

                    signal startEditing()
                    signal finishEditing()

                    sourceComponent: treeView.delegate

                    anchors.left: button.right
                    anchors.right: parent.right

                    Connections
                    {
                        target: selectionModel
                        enabled: delegateLoader.isCurrent
                        onSelectionChanged: delegateLoader.finishEditing()
                    }
                }
            }
        }

        Rectangle
        {
            // TODO: #vkutin Limit the size of displayed list.

            id: dragIndicator

            width: column.width
            height: column.height
            color: ColorTheme.transparent("black", 0.8)
            visible: false

            Rectangle
            {
                anchors.fill: parent
                color: selectionHighlightColor
            }

            Column
            {
                id: column

                Repeater
                {
                    model: IndexListModel
                    {
                        id: indexListModel
                        source: selectionModel.selectedIndexes
                    }

                    delegate: Component
                    {
                        Item
                        {
                            id: draggedItem

                            width: dragDelegateLoader.x + dragDelegateLoader.width + 12
                            height: dragDelegateLoader.height
                            visible: itemFlags & Qt.ItemIsDragEnabled

                            readonly property var modelData: model
                            readonly property var modelIndex: indexListModel.sourceIndex(index)

                            readonly property int itemFlags:
                                linearizationListModel.flags(modelIndex)

                            Loader
                            {
                                id: dragDelegateLoader

                                readonly property var model: draggedItem.modelData
                                readonly property var modelIndex:
                                    linearizationListModel.mapToSource(draggedItem.modelIndex)

                                readonly property int itemFlags: draggedItem.itemFlags
                                readonly property bool isCurrent: false
                                readonly property bool isSelected: true
                                readonly property real itemIndent: x

                                sourceComponent: treeView.delegate
                                x: 24

                                // Signals for compatibility with the underlying delegate.
                                signal startEditing()
                                signal finishEditing()
                            }
                        }
                    }
                }
            }
        }
    }
}
