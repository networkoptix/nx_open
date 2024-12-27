// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Layouts
import QtQuick.Window

import Nx.Controls
import Nx.Core
import Nx.Core.Controls
import Nx.Dialogs
import Nx.RightPanel

import nx.vms.client.core
import nx.vms.client.desktop
import nx.vms.client.desktop.analytics as Analytics

Dialog
{
    id: control

    title: qsTr("Settings")

    modality: Qt.ApplicationModal

    property alias attributeManager: settingsModel.attributeManager
    property alias objectTypeIds: settingsModel.objectTypeIds

    minimumWidth: 320
    minimumHeight: 440

    width: minimumWidth
    height: minimumHeight

    Analytics.AttributeVisibilitySettingsModel
    {
        id: settingsModel
    }

    SortFilterProxyModel
    {
        id: filterModel
        sourceModel: settingsModel
        filterRegularExpression: new RegExp(NxGlobals.escapeRegExp(searchEdit.text), "i")
    }

    ColumnLayout
    {
        id: content

        anchors.fill: parent
        anchors.bottomMargin: buttonBox.height

        Rectangle
        {
            id: searchRectangle

            Layout.fillWidth: true
            height: 60
            color: ColorTheme.colors.dark8

            SearchField
            {
                id: searchEdit

                x: 16
                y: 16
                width: parent.width - x * 2
            }
        }

        ListView
        {
            id: objectList

            Layout.topMargin: 16
            Layout.fillWidth: true
            Layout.fillHeight: true

            clip: true
            model: filterModel
            spacing: 6

            readonly property int itemMargin: 16
            readonly property int itemHeight: 28

            property int hoveredItemIndex: -1
            property Item dragItem: null
            property var position: null

            onDragItemChanged:
            {
                if (objectList.dragItem)
                {
                    const position = objectList.mapFromItem(dragItem, dragItem.x, dragItem.y)
                    draggableItem.x = objectList.itemMargin
                    draggableItem.y = position.y
                }
            }

            PersistentIndexWatcher
            {
                id: dragIndexWatcher
            }

            delegate: Item
            {
                id: delegateItem

                height: objectList.itemHeight
                width: objectList.width

                readonly property var modelIndex:
                    NxGlobals.toPersistent(objectList.model.index(index, 0))
                readonly property int itemFlags: itemFlagsWatcher.itemFlags

                ModelItemFlagsWatcher
                {
                    id: itemFlagsWatcher
                    index: delegateItem.modelIndex
                }

                Rectangle
                {
                    id: listItem

                    anchors.fill: parent
                    anchors.leftMargin: objectList.itemMargin
                    anchors.rightMargin: objectList.itemMargin
                    color: ColorTheme.colors.dark8

                    readonly property Item shutter: shutter
                    readonly property bool checkable: itemFlags & Qt.ItemIsUserCheckable
                    readonly property bool checked: model.checked
                    readonly property string text: model.display
                    readonly property bool draggable: itemFlags & Qt.ItemIsDragEnabled

                    RowLayout
                    {
                        spacing: 2
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8

                        CheckBox
                        {
                            id: itemCheckBox

                            Layout.alignment: Qt.AlignBaseline
                            Layout.fillWidth: true
                            focusPolicy: Qt.NoFocus
                            enabled: listItem.checkable
                            checked: listItem.checked
                            elide: Text.ElideRight
                            textFormat: Text.StyledText
                            text: searchEdit.text
                                ? NxGlobals.highlightMatch(
                                    listItem.text,
                                    filterModel.filterRegularExpression,
                                    ColorTheme.colors.yellow_d1)
                                : listItem.text

                            onToggled: model.checked = checked
                        }

                        ColoredImage
                        {
                            id: dragImage

                            enabled: listItem.draggable

                            sourcePath: "image://skin/20x20/Outline/drag.svg"
                            primaryColor: enabled ? "light10" : "dark13"

                            MouseArea
                            {
                                id: mouseArea

                                anchors.fill: parent
                                acceptedButtons: Qt.LeftButton
                                drag.smoothed: false

                                onPressed:
                                {
                                    objectList.dragItem = listItem
                                    dragIndexWatcher.index = delegateItem.modelIndex
                                    drag.target = draggableItem
                                }

                                onReleased: drop()
                                onCanceled: cancelDrag()

                                drag.onActiveChanged:
                                {
                                    if (drag.active)
                                        shutter.visible = true
                                    else
                                        cancelDrag()
                                }

                                onPositionChanged: (mouse) =>
                                {
                                    if (mouseArea.drag.active)
                                    {
                                        objectList.position =
                                            mapToItem(objectList, mouse.x, mouse.y)
                                        relativePositionChanged()
                                    }
                                }

                                Connections
                                {
                                    target: objectList

                                    function onContentYChanged()
                                    {
                                        if (mouseArea.drag.active)
                                            mouseArea.relativePositionChanged()
                                    }
                                }

                                Connections
                                {
                                    target: dragIndexWatcher
                                    enabled: mouseArea.drag.active

                                    function onIndexChanged()
                                    {
                                        mouseArea.cancelDrag()
                                    }
                                }

                                function relativePositionChanged()
                                {
                                    if (drag.active)
                                    {
                                        autoScroller.velocity = ProximityScrollHelper.velocity(
                                            Qt.rect(0, 0, objectList.width, objectList.height),
                                            objectList.position)

                                        let y = objectList.position.y + objectList.contentY
                                        y -= y % (objectList.itemHeight + objectList.spacing)
                                        const hoveredItem = objectList.itemAt(
                                            objectList.position.x, y)
                                        objectList.hoveredItemIndex =
                                            hoveredItem
                                            && hoveredItem.itemFlags & Qt.ItemIsDropEnabled
                                                ? hoveredItem.modelIndex.row
                                                : -1
                                    }
                                }

                                function drop()
                                {
                                    autoScroller.velocity = 0
                                    if (!drag.active
                                        || objectList.hoveredItemIndex < 0
                                        || !DragAndDrop.drop(
                                            DragAndDrop.createMimeData([dragIndexWatcher.index]),
                                            Qt.MoveAction,
                                            objectList.model.index(objectList.hoveredItemIndex, 0)))
                                    {
                                        cancelDrag()
                                    }
                                }

                                function cancelDrag()
                                {
                                    autoScroller.velocity = 0
                                    objectList.hoveredItemIndex = -1
                                    if (drag.active)
                                        draggableItem.goHome()
                                    drag.target = null
                                }
                            }
                        }
                    }
                }

                Rectangle
                {
                    id: shutter

                    anchors.fill: listItem
                    color: ColorTheme.colors.dark7
                    visible: false
                }
            }

            Rectangle
            {
                id: dropMarker

                readonly property bool allowed: objectList.dragItem
                    && objectList.hoveredItemIndex >= 0
                    && objectList.hoveredItemIndex !== dragIndexWatcher.index.row
                visible: allowed
                width: objectList.width - objectList.itemMargin * 2
                height: 2
                color: ColorTheme.colors.brand_core
                x: objectList.itemMargin
                y:
                {
                    if (!allowed)
                        return -1

                    const hoveredItem = objectList.itemAtIndex(objectList.hoveredItemIndex)
                    if (!hoveredItem)
                        return -1

                    return (objectList.hoveredItemIndex < dragIndexWatcher.index.row
                        ? hoveredItem.y - (objectList.spacing + height) / 2
                        : hoveredItem.y + hoveredItem.height + (objectList.spacing - height) / 2)
                        - objectList.contentY
                }
            }

            Rectangle
            {
                id: draggableItem

                width: objectList.width - objectList.itemMargin * 2
                x: objectList.itemMargin
                z: 10
                height: objectList.itemHeight
                color: ColorTheme.colors.dark8
                visible: !!objectList.dragItem

                RowLayout
                {
                    spacing: 2
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8

                    CheckBox
                    {
                        Layout.alignment: Qt.AlignBaseline
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                        enabled: objectList.dragItem?.checkable ?? false
                        checked: objectList.dragItem?.checked ?? false
                        text: objectList.dragItem?.text ?? ""
                        textFormat: Text.StyledText
                    }

                    ColoredImage
                    {
                        enabled: objectList.dragItem?.draggable ?? false
                        sourcePath: "image://skin/20x20/Outline/drag.svg"
                        primaryColor: enabled ? "light10" : "dark13"
                    }
                }

                ParallelAnimation
                {
                    id: goHomeAnimation


                    NumberAnimation
                    {
                        id: horizontalAnimation

                        target: draggableItem
                        properties: "x"
                        from: draggableItem.x
                        to: objectList.itemMargin
                        easing.type: Easing.InOutQuad
                        duration: 300
                    }

                    NumberAnimation
                    {
                        id: verticalAnimation

                        target: draggableItem
                        properties: "y"
                        from: draggableItem.y
                        easing.type: Easing.InOutQuad
                        duration: 300
                    }

                    onFinished:
                    {
                        finish()
                    }

                    function finish()
                    {
                        if (objectList.dragItem)
                        {
                            objectList.dragItem.shutter.visible = false
                            objectList.dragItem = null
                        }
                    }
                }

                function goHome()
                {
                    const item = objectList.itemAtIndex(dragIndexWatcher.index.row)
                    if (item)
                    {
                        verticalAnimation.to = item.y - objectList.contentY
                        goHomeAnimation.start()
                    }
                    else
                    {
                        goHomeAnimation.finish()
                    }
                }
            }

            AutoScroller
            {
                id: autoScroller

                scrollBar: objectList.scrollBar
                contentSize: objectList.contentHeight
            }

            Connections
            {
                id: modelSlots

                target: settingsModel

                property real storedPosition: 0

                function onRowsAboutToBeMoved()
                {
                    modelSlots.storedPosition = objectList.contentY
                }

                function onRowsMoved()
                {
                    Qt.callLater(function() { objectList.contentY = modelSlots.storedPosition })
                }
            }
        }
    }

    buttonBox: DialogButtonBox
    {
        buttonLayout: DialogButtonBox.KdeLayout
        standardButtons: DialogButtonBox.Ok
    }
}
