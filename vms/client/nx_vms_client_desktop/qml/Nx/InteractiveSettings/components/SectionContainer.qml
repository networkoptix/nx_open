// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.0
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.10

import Nx.Controls 1.0

import "private"

/**
 * Interactive Settings type.
 */
StackLayout
{
    id: control

    property string name: ""
    property string caption: ""
    property Item childrenItem: column
    property StackLayout sectionStack: control
    property Item scrollBarParent: null
    property alias verticalScrollBar: view.verticalScrollBar
    property Item extraHeaderItem: null
    property alias placeholderItem: placeholderContainer.contentItem

    property bool contentEnabled: parent && parent.hasOwnProperty("contentEnabled")
        ? parent.contentEnabled
        : true

    property bool contentVisible: true

    readonly property bool hasOtherSections: control.children.length > 1
    readonly property bool hasItems: childrenItem.layoutItems.length > 0
    readonly property bool isEmpty: !hasOtherSections && !hasItems

    property bool fillWidth: true
    property int heightHint: currentIndex > 1
        ? children[currentIndex].heightHint
        : view.contentItem.implicitHeight

    Scrollable
    {
        id: view

        Layout.minimumWidth: 200 //< Needed to get implicit height calculated correctly.

        verticalScrollBar: ScrollBar
        {
            parent: scrollBarParent ? scrollBarParent : view.scrollView
            x: parent ? parent.width - width : 0
            height: parent ? parent.height : 0
            visible: view.visible
            opacity: enabled ? 1.0 : 0.3
            enabled: size < 0.9999
        }

        contentItem: Column
        {
            width:
            {
                return view.verticalScrollBar && view.verticalScrollBar.parent === view
                    ? Math.min(view.width, view.verticalScrollBar.x)
                    : view.width
            }

            Item
            {
                id: headerContainer

                width: parent.width
                height: extraHeaderItem ? extraHeaderItem.height : 0

                Binding
                {
                    target: extraHeaderItem
                    property: "width"
                    value: headerContainer.width
                }

                Binding
                {
                    target: extraHeaderItem
                    property: "parent"
                    value: headerContainer
                }
            }

            Control
            {
                id: placeholderContainer
                width: parent.width
                height: view.height - y
                visible: control.isEmpty && control.contentVisible
                onContentItemChanged: contentItem.parent = placeholderContainer
            }

            Item
            {
                id: spacer
                height: 32
                width: 1
                visible: extraHeaderItem && control.hasItems
            }

            LabeledColumnLayout
            {
                id: column

                spacing: 16
                preferredHeight: view.height - y
                width: parent.width
                enabled: control.contentEnabled
                visible: control.contentVisible && control.hasItems
            }
        }
    }

    onCurrentIndexChanged:
    {
        if (currentIndex > 0 && children[currentIndex].hasOwnProperty("scrollBarParent"))
            children[currentIndex].scrollBarParent = scrollBarParent
    }
}
