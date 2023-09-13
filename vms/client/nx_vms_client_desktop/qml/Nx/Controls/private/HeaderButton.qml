// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQml
import QtQuick
import QtQuick.Controls

import Nx
import Nx.Controls
import Nx.Core

import nx.vms.client.desktop

Button
{
    id: control

    enum Order
    {
        AscendingOrder,
        DescendingOrder,
        NoOrder
    }

    property var sortOrder: HeaderButton.NoOrder
    property bool isCheckbox: false

    property var _inputCheckState: Qt.Unchecked // Private, internal-usage only.

    signal checkStateChanged(int checkState)

    function setCheckState(checkState)
    {
        _inputCheckState = checkState
    }

    leftPadding: 8
    rightPadding: 8

    clip: true

    backgroundColor: "transparent"
    hoveredColor: ColorTheme.transparent(ColorTheme.colors.dark9, 0.2)
    pressedColor: hoveredColor

    textColor: ColorTheme.colors.light4
    font.pixelSize: 14
    font.weight: Font.Medium

    iconUrl:
    {
        if (sortOrder === HeaderButton.AscendingOrder)
            return "image://svg/skin/table_view/ascending.svg"

        if (sortOrder === HeaderButton.DescendingOrder)
            return "image://svg/skin/table_view/descending.svg"

        return ""
    }

    contentItem: Loader
    {
        sourceComponent: control.isCheckbox ? checkboxComponent : textComponent

        Component
        {
            id: checkboxComponent

            CheckBox
            {
                id: checkBox

                font: control.font

                checkState: control._inputCheckState
                onCheckStateChanged:
                {
                    if (checkState !== control._inputCheckState)
                        control.checkStateChanged(checkState)
                }
            }
        }

        Component
        {
            id: textComponent

            Item
            {
                implicitWidth:
                    label.implicitWidth + iconItem.anchors.leftMargin + iconItem.implicitWidth

                implicitHeight: Math.max(label.height, iconItem.height)

                Text
                {
                    id: label

                    width: Math.min(implicitWidth, parent.width - iconItem.width - 4)
                    anchors.verticalCenter: parent.verticalCenter

                    text: control.text

                    color: control.textColor
                    font: control.font
                    elide: Text.ElideRight
                }

                IconImage
                {
                    id: iconItem

                    anchors.left: label.right
                    anchors.leftMargin: 4
                    anchors.verticalCenter: parent.verticalCenter

                    source: control.icon.source
                    sourceSize: Qt.size(control.icon.width, control.icon.height)

                    onSourceChanged:
                    {
                        // It is needed for correct implicitWidth recalculation.
                        label.elide = Text.ElideNone
                        width = Math.min(implicitWidth, parent.width - iconItem.width - 4)
                        label.elide = Text.ElideRight
                    }

                    visible: !!source && width > 0 && height > 0

                    width: source != "" ? control.icon.width : 0
                    height: source != "" ? control.icon.height : 0

                    color: control.textColor
                }
            }
        }
    }
}
