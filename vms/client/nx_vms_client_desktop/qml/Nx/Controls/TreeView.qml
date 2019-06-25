import QtQuick 2.6
import QtQml.Models 2.11

import Nx.Controls 1.0

import nx.vms.client.desktop 1.0

/**
 * A tree view with multi-selection.
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

        ItemSelectionHelper
        {
            id: selectionHelper
        }

        delegate: Component
        {
            Item
            {
                id: treeItem

                width: parent.width
                implicitHeight: Math.max(button.implicitHeight, delegateLoader.implicitHeight)

                readonly property bool isSelectable: itemFlags & Qt.ItemIsSelectable
                readonly property bool isEditable: itemFlags & Qt.ItemIsEditable

                readonly property var modelData: model
                readonly property var modelIndex: linearizationListModel.index(index, 0)
                readonly property int itemFlags: linearizationListModel.flags(modelIndex)
                readonly property bool isCurrent: ListView.isCurrentItem
                readonly property bool isSelected: selectionHighlight.visible

                Rectangle
                {
                    id: selectionHighlight

                    anchors.fill: parent
                    visible: false

                    readonly property bool isDelegateFocused: delegateLoader.item
                        && delegateLoader.item.activeFocus

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
                                var selection = selectionHelper.createSelection(
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
                                reselectOnRelease = true
                            }
                        }
                    }

                    onReleased:
                    {
                        if (!reselectOnRelease)
                            return

                        reselectOnRelease = false

                        if (modelIndex.valid && !drag.active)
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
    }
}
