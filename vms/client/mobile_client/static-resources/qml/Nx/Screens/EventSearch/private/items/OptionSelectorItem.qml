// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core
import Nx.Controls
import Nx.Mobile.Controls
import Nx.Ui

/**
 * Content of the option selection screen. Hosts `selector.screenDelegate` in a flickable area and
 * forwards `apply()`, `clear()` and the `applyRequested` signal to it. Opening and closing the
 * screen is up to the host.
 *
 * Every member of the delegate interface is optional:
 *     - `selector` property: receives the current OptionSelector.
 *     - `searchEdit` property: receives the header SearchEdit, the delegate controls its
 *       visibility. Without this property the search field is hidden.
 *     - `setValue(value)`: called with the selector value once the delegate is loaded.
 *     - `apply()`: commits the delegate state to the selector.
 *     - `clear()`: resets the selected value.
 *     - `applyRequested` signal: apply immediately and close the screen. Its presence also makes
 *       `closesOnApply` true.
 */
Item
{
    id: optionSelectorItem

    property string title: selector ? selector.descriptionText : ""
    property OptionSelector selector: null

    // Whether the active screen delegate auto-closes on a single-item selection (i.e. emits
    // `applyRequested`). Used by the host screen to mirror this auto-close behavior on Reset.
    readonly property bool closesOnApply:
        !!delegateLoader.item && delegateLoader.item.hasOwnProperty("applyRequested")

    function apply()
    {
        d.callDelegateFunction("apply")
    }

    function clear()
    {
        d.callDelegateFunction("clear")
    }

    signal applyRequested

    FocusScope
    {
        anchors.fill: parent

        SearchEdit
        {
            id: searchEdit

            anchors.left: parent.left
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.leftMargin: 20
            anchors.topMargin: LayoutController.hasSidePanels ? 0 : 20
            anchors.rightMargin: 20

            height: 36
            visible: false
        }

        Flickable
        {
            id: flickable

            anchors
            {
                left: parent.left
                top: searchEdit.visible ? searchEdit.bottom : parent.top
                right: parent.right
                bottom: parent.bottom

                topMargin: (searchEdit.visible || !LayoutController.hasSidePanels) ? 20 : 0
                leftMargin: 20
                rightMargin: 20
            }

            contentHeight: delegateLoader.height
            clip: true

            Loader
            {
                id: delegateLoader

                width: flickable.width

                sourceComponent: selector && selector.screenDelegate

                onItemChanged:
                {
                    searchEdit.clear()

                    if (!item)
                        return

                    if (item.hasOwnProperty("searchEdit"))
                        item.searchEdit = searchEdit
                    else
                        searchEdit.visible = false

                    if (item.hasOwnProperty("selector"))
                        item.selector = optionSelectorItem.selector

                    if (item.hasOwnProperty("setValue"))
                        item.setValue(selector.value)

                    if (item.hasOwnProperty("applyRequested"))
                        item.applyRequested.connect(optionSelectorItem.applyRequested)
                }
            }
        }
    }

    NxObject
    {
        id: d

        function callDelegateFunction(functionName)
        {
            const delegateItem = delegateLoader.item
            if (delegateItem && delegateItem.hasOwnProperty(functionName))
                delegateItem[functionName]()
        }
    }
}
