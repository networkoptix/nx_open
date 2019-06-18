import QtQuick 2.6

import Nx.Controls 1.0

import nx.vms.client.desktop 1.0

/**
 * A tree view.
 *
 * To control which items are hoverable/selectable, the delegate must have "selectable" property.
 * 
 * Mouse click selection is implemented.
 */
Item
{
    id: treeView

    property var model: null
    property Component delegate: null
    property real indentation: 24 //< Horizontal indent of next tree level, in pixels.

    property alias scrollStepSize: listView.scrollStepSize //< In pixels.
    property alias selectionHighlightColor: listView.selectionHighlightColor
    property alias hoverHighlightColor: listView.hoverHighlightColor

    ListView
    {
        id: listView

        anchors.fill: parent

        model: LinearizationListModel { sourceModel: treeView.model }

        delegate: Item
        {
            property bool selectable: !delegateLoader.item
                || delegateLoader.item.selectable === undefined
                || delegateLoader.item.selectable

            width: parent.width
            implicitHeight: Math.max(button.implicitHeight, delegateLoader.implicitHeight)

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

                onClicked:
                {
                    var toggle = model && model.hasChildren
                        && button.contains(mapToItem(button, mouse.x, mouse.y))

                    if (toggle)
                        model.expanded = !model.expanded
                    else
                        listView.currentIndex = index
                }
            }

            readonly property var modelData: model
            readonly property bool isCurrent: ListView.isCurrentItem

            Loader
            {
                id: delegateLoader

                readonly property var model: modelData
                readonly property ListView view: listView
                readonly property bool isCurrentItem: isCurrent
                readonly property real itemIndent: x

                sourceComponent: treeView.delegate

                anchors.left: button.right
                anchors.right: parent.right
            }
        }
    }
}
