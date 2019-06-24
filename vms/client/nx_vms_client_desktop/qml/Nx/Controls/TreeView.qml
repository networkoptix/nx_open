import QtQuick 2.6
import QtQml.Models 2.11

import Nx.Controls 1.0

import nx.vms.client.desktop 1.0

/**
 * A tree view with multi-selection.
 *
 * To control which items are hoverable/selectable, the delegate must have "selectable" property.
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
                property bool selectable: !delegateLoader.item
                    || delegateLoader.item.selectable === undefined
                    || delegateLoader.item.selectable

                width: parent.width
                implicitHeight: Math.max(button.implicitHeight, delegateLoader.implicitHeight)

                Rectangle
                {
                    id: selectionHighlight

                    anchors.fill: parent
                    color: selectionHighlightColor
                    visible: false

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

                        var toggle = model && model.hasChildren
                            && button.contains(mapToItem(button, mouse.x, mouse.y))

                        if (toggle)
                        {
                            model.expanded = !model.expanded
                            return
                        }

                        if (selectable)
                        {
                            var modelIndex = linearizationListModel.index(index, undefined)
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
                            else
                            {
                                selectionModel.setCurrentIndex(
                                    modelIndex, ItemSelectionModel.ClearAndSelect)
                            }
                        }
                    }
                }

                readonly property var modelData: model
                readonly property bool isCurrent: ListView.isCurrentItem

                Loader
                {
                    id: delegateLoader

                    readonly property var model: modelData
                    readonly property bool isCurrentItem: isCurrent
                    readonly property real itemIndent: x
                    readonly property var modelIndex:
                        linearizationListModel.mapToSource(linearizationListModel.index(index, 0))

                    sourceComponent: treeView.delegate

                    anchors.left: button.right
                    anchors.right: parent.right
                }
            }
        }
    }
}
