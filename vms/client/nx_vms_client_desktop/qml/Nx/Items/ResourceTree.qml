import QtQuick 2.6

import Nx 1.0
import Nx.Controls 1.0
import Nx.Layout 1.0
import Nx.Models 1.0

import nx.vms.client.desktop 1.0

TreeView
{
    readonly property real kRowHeight: 20
    readonly property real kSeparatorRowHeight: 16

    property LayoutViewer scene: null

    indentation: 24
    scrollStepSize: kRowHeight
    hoverHighlightColor: ColorTheme.transparent(ColorTheme.colors.dark8, 0.4)
    selectionHighlightColor: ColorTheme.transparent(ColorTheme.colors.brand_core, 0.3)

    supportedDragActions: Qt.CopyAction | Qt.MoveAction
    proposedDragAction: Qt.CopyAction

    model: ResourceTreeModel
    {
        id: resourceTreeModel
    }

    ModelDataAccessor
    {
        id: modelDataAccessor
        model: resourceTreeModel
    }

    delegate: Component
    {
        Loader
        {
            id: delegateItem

            readonly property int nodeType: (model && model.nodeType) || -1

            readonly property bool isSeparator: nodeType == ResourceTree.NodeType.separator
                || nodeType == ResourceTree.NodeType.localSeparator

            property bool isEditing: false
            property TextInput activeEditor: null

            focus: isEditing

            Connections
            {
                target: delegateItem.parent

                onStartEditing:
                    isEditing = true

                onFinishEditing:
                {
                    if (activeEditor)
                        activeEditor.onEditingFinished()

                    isEditing = false
                }
            }

            height: item && item.height

            sourceComponent:
            {
                if (isSeparator)
                    return separatorComponent

                if (isEditing)
                    return editorComponent

                return normalComponent
            }

            Component
            {
                id: normalComponent

                Row
                {
                    height: kRowHeight
                    spacing: 4

                    Image
                    {
                        id: icon

                        width: kIconWidth
                        height: parent.height
                        fillMode: Image.Stretch
                        source: iconSource
                    }

                    Text
                    {
                        id: name

                        text: (model && model.display) || ""
                        font.weight: Font.DemiBold
                        height: parent.height
                        verticalAlignment: Text.AlignVCenter
                        color: mainTextColor
                    }

                    Text
                    {
                        id: extraInfo

                        text: (model && model.extraInfo) || ""
                        font.weight: Font.Normal
                        height: parent.height
                        verticalAlignment: Text.AlignVCenter
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
                }
            }

            Component
            {
                id: editorComponent

                Row
                {
                    height: kRowHeight
                    spacing: 4

                    Image
                    {
                        id: icon

                        width: kIconWidth
                        height: parent.height
                        fillMode: Image.Stretch
                        source: iconSource
                    }

                    TextInput
                    {
                        id: nameEditor

                        focus: true
                        text: (model && model.display) || ""
                        font.weight: Font.DemiBold
                        width: parent.width - x
                        height: parent.height
                        verticalAlignment: Text.AlignVCenter
                        selectionColor: selectionHighlightColor
                        selectByMouse: true
                        selectedTextColor: mainTextColor
                        color: mainTextColor

                        Keys.onEscapePressed:
                            delegateItem.isEditing = false

                        onEditingFinished:
                        {
                            if (model && delegateItem.isEditing)
                                model.edit = text

                            delegateItem.isEditing = false
                        }

                        Component.onCompleted:
                        {
                            delegateItem.activeEditor = nameEditor
                            forceActiveFocus()
                            selectAll()
                        }
                    }
                }
            }

            Component
            {
                id: separatorComponent

                Item
                {
                    width: parent.width
                    height: kSeparatorRowHeight

                    Rectangle
                    {
                        id: separatorLine

                        height: 1
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.leftMargin: -itemIndent

                        color: ColorTheme.transparent(ColorTheme.colors.dark8, 0.4)
                    }
                }
            }

            readonly property int kIconWidth: 20

            readonly property string iconSource:
                (model && model.iconKey && model.iconKey !== 0
                    && ("image://resource/" + model.iconKey + "/" + itemState)) || ""

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

                        if (!itemLayout || scene.layout !== itemLayout)
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

                        // TODO: #vkutin handle videowalls.

                        if (scene.currentResource === resource)
                            return ResourceTree.ItemState.accented

                        if (scene.layout === resource || scene.containsResource(resource))
                            return ResourceTree.ItemState.selected

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

                            hasSelectedChildren |= scene.containsResource(childResource)
                        }

                        return hasSelectedChildren
                            ? ResourceTree.ItemState.selected
                            : ResourceTree.ItemState.normal
                    }

                    // TODO: #vkutin handle these.
                    case ResourceTree.NodeType.videoWallItem:
                    case ResourceTree.NodeType.layoutTour:
                        return ResourceTree.ItemState.normal
                }

                return ResourceTree.ItemState.normal
            }
        }
    }
}
