// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.0
import QtQuick.Layouts 1.10

import Nx.Controls 1.0

import "private"

StackLayout
{
    id: control

    property string caption: ""
    property Item childrenItem: column
    property StackLayout sectionStack: control
    property Item scrollBarParent: null
    property alias verticalScrollBar: view.verticalScrollBar
    property Item extraHeaderItem: null

    property bool contentEnabled: parent && parent.hasOwnProperty("contentEnabled")
        ? parent.contentEnabled
        : true

    property bool contentVisible: true

    Scrollable
    {
        id: view

        verticalScrollBar: ScrollBar
        {
            parent: scrollBarParent ? scrollBarParent : view
            x: parent ? parent.width - width : 0
            height: parent ? parent.height : 0
            visible: view.visible
            opacity: enabled ? 1.0 : 0.3
            enabled: size < 0.9999
        }

        contentItem: Column
        {
            spacing: 32

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

            AlignedColumn
            {
                id: column

                spacing: 16

                width: parent.width
                enabled: control.contentEnabled
                visible: control.contentVisible
            }
        }
    }

    onCurrentIndexChanged:
    {
        if (currentIndex > 0 && children[currentIndex].hasOwnProperty("scrollBarParent"))
            children[currentIndex].scrollBarParent = scrollBarParent
    }
}
