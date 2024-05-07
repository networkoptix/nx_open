// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Layouts
import QtQuick.Window

import Nx
import Nx.Controls
import Nx.Core
import Nx.Dialogs
import Nx.RightPanel

import nx.vms.client.core
import nx.vms.client.desktop
import nx.vms.client.desktop.analytics as Analytics

Dialog
{
    id: control

    title: qsTr("Settings")

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

            delegate: Item
            {
                id: delegateItem

                x: 16
                height: 28
                width: objectList.width - 2 * x

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
                    color: ColorTheme.colors.dark8

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
                            enabled: itemFlags & Qt.ItemIsUserCheckable
                            checked: model.checked
                            elide: Text.ElideRight
                            textFormat: Text.StyledText
                            text: searchEdit.text
                                ? NxGlobals.highlightMatch(model.display,
                                    filterModel.filterRegularExpression, ColorTheme.colors.yellow_d1)
                                : model.display

                            onToggled: model.checked = checked
                        }

                        ColoredImage
                        {
                            id: dragImage

                            enabled: itemFlags & Qt.ItemIsDragEnabled

                            sourcePath: "image://skin/advanced_search_dialog/icon_drag.svg"
                            primaryColor: enabled ? "light10" : "dark13"

                            MouseArea
                            {
                                id: mouseArea

                                anchors.fill: parent
                                acceptedButtons: Qt.LeftButton

                                drag.axis: Drag.YAxis
                                drag.target: listItem

                                drag.onActiveChanged:
                                {
                                    if (drag.active)
                                    {
                                        ItemGrabber.grabToImage(listItem,
                                            function (resultUrl)
                                            {
                                                if (!mouseArea.drag.active)
                                                    return

                                                DragAndDrop.execute(
                                                    listItem,
                                                    DragAndDrop.createMimeData([modelIndex]),
                                                    [Qt.MoveAction],
                                                    Qt.MoveAction,
                                                    resultUrl,
                                                    Qt.point(
                                                        dragImage.x + dragImage.width,
                                                        dragImage.y + dragImage.height / 2))
                                            })
                                    }
                                }
                            }
                        }
                    }
                }

                DropArea
                {
                    id: dropArea

                    anchors.fill: parent
                    anchors.topMargin: -objectList.spacing / 2
                    anchors.bottomMargin: -objectList.spacing / 2
                    onEntered: (drag) =>
                    {
                        drag.accepted = (itemFlags & Qt.ItemIsDropEnabled)
                    }

                    onDropped: (drop) =>
                    {
                        drop.accepted = (itemFlags & Qt.ItemIsDropEnabled)
                            && DragAndDrop.drop(currentMimeData, drop.action, modelIndex)
                    }

                    Rectangle
                    {
                        id: dropHighlight

                        anchors.fill: parent

                        color: "transparent"
                        border.color: ColorTheme.transparent(ColorTheme.colors.brand_core, 0.6)
                        border.width: 1
                        radius: 2
                        visible: dropArea.containsDrag
                    }
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
