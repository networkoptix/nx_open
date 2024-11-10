// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15

import Nx.Core 1.0
import Nx.Mobile.Controls 1.0

import "./helpers.js" as ValueHelpers

/**
 * Basic implementation for the grid selector screen delegate. Can be tuned with the cell visual
 * delegate
 */
Grid
{
    id: control

    property OptionSelector selector: null

    property Component visualDelegate: null //< Visual delegate for the cells.

    property alias model: repeater.model

    property string valueRoleId //< Role name for the value to be extracted from the model.

    signal applyRequested()

    function setValue(newValue)
    {
        d.selectedIndex = ValueHelpers.getIndex(newValue, model, selector.compareFunc)
    }

    function clear()
    {
        d.selectedIndex = -1
    }

    columns: Math.floor((width - 16 * 2) / d.kItemWidth)
    spacing: 8
    leftPadding: (width - (columns * d.kItemWidth + (columns - 1) * spacing)) / 2

    Repeater
    {
        id: repeater

        delegate: Rectangle
        {
            id: item

            readonly property bool selected: index === d.selectedIndex

            width: d.kItemWidth
            height: d.kItemHeight

            radius: 8
            border.width: 2
            border.color: item.selected
                ? ColorTheme.colors.light16
                : ColorTheme.colors.dark10
            color: item.selected
                ? ColorTheme.colors.dark9
                : ColorTheme.colors.dark7

            Column
            {
                anchors.centerIn: parent

                spacing: 16

                Loader
                {
                    id: visualDelegateLoader

                    sourceComponent: control.visualDelegate
                    anchors.horizontalCenter: parent.horizontalCenter

                    Binding
                    {
                        target: visualDelegateLoader.item
                        when: visualDelegateLoader.status === Loader.Ready
                            && visualDelegateLoader.item.hasOwnProperty("data")
                        property: "data"
                        value: modelData
                    }

                    Binding
                    {
                        target: visualDelegateLoader.item
                        when: visualDelegateLoader.status === Loader.Ready
                            && visualDelegateLoader.item.hasOwnProperty("textValueItem")
                        property: "textValueItem"
                        value: textItem
                    }
                }

                Text
                {
                    id: textItem

                    anchors.horizontalCenter: parent.horizontalCenter
                    font.pixelSize: 16
                    color: item.selected
                        ? ColorTheme.colors.light1
                        : ColorTheme.colors.light10
                    text: d.getTextValue(index)
                    elide: Text.ElideRight
                    width: Math.min(textItem.implicitWidth, d.kMaxTextWidth)
                    maximumLineCount: 2
                    wrapMode: Text.Wrap
                    horizontalAlignment: Text.AlignHCenter
                }
            }

            MouseArea
            {
                anchors.fill: parent
                onClicked:
                {
                    d.selectedIndex = index
                    control.applyRequested()
                }
            }
        }
    }

    NxObject
    {
        id: d

        readonly property int kItemWidth: 109
        readonly property int kItemHeight: 116
        readonly property int kTextPadding: 16
        readonly property int kMaxTextWidth: kItemWidth - 2 * kTextPadding

        property int selectedIndex: getCurrentIndex()

        function getCurrentIndex()
        {
            return control.selector
                ? ValueHelpers.getIndex(control.selector.value, control.model, selector.compareFunc)
                : -1
        }

        function getTextValue(index)
        {
            if (!control.selector)
                return ""

            return ValueHelpers.getTextValue(index,
                control.model,
                control.selector.unselectedValue,
                control.selector.unselectedTextValue,
                control.valueRoleId,
                control.selector.textFieldName,
                control.selector.valueToTextFunc)
        }

        Connections
        {
            target: control.selector

            function onValueChanged()
            {
                const newIndex = d.getCurrentIndex()
                if (newIndex !== d.selectedIndex)
                    d.selectedIndex = newIndex
            }
        }

        Binding
        {
            restoreMode: Binding.RestoreNone
            target: selector
            property: "value"
            value: ValueHelpers.getValue(
                d.selectedIndex,
                control.model,
                control.selector.unselectedValue,
                control.valueRoleId)
        }

        Binding
        {
            restoreMode: Binding.RestoreNone
            target: control.selector
            property: "textValue"
            value: d.getTextValue(d.selectedIndex)
        }

        Binding
        {
            restoreMode: Binding.RestoreNone
            target: control.selector
            property: "isDefaultValue"
            value: d.selectedIndex === -1
        }
    }
}
