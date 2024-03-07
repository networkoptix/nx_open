// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx
import Nx.Controls

import "private"

/**
 * This component is intended to be statically placed among menu items, and while taking no space
 * itself it will populate the menu after its own position with items, separators and sub-menus,
 * instantiated in accordance with the specified tree model.
 *
 * For planar models without separators usually a simple Repeater is enough.
 */
Item
{
    id: item

    required property Menu parentMenu

    property AbstractItemModel model
    property var rootIndex: NxGlobals.invalidModelIndex()

    property Component separator: MenuSeparator {}

    property string textRole: "text"
    property string dataRole: "data"
    property string decorationPathRole: "iconPath"
    property string shortcutRole: "shortcut"
    property string enabledRole: "enabled"
    property string visibleRole: "visible"

    readonly property int count: instantiator.count

    visible: false

    DynamicMenuItemInstantiator
    {
        id: instantiator

        parentMenu: item.parentMenu
        indexOffset: Array.prototype.indexOf.call(item.parent.children, item) + 1
        sourceModel: item.model
        rootIndex: item.rootIndex
        separator: item.separator
        textRole: item.textRole
        dataRole: item.dataRole
        decorationPathRole: item.decorationPathRole
        shortcutRole: item.shortcutRole
        enabledRole: item.enabledRole
        visibleRole: item.visibleRole
    }
}
