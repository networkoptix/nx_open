// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core
import Nx.Controls
import Nx.Mobile.Controls
import Nx.Ui

// TODO: update comment below.
/**
 * Option selection underlying screen implementation. Works in ties with OptionSelector component.
 * Implements basic "apply", "open" and "close functionality" and flickable central area which
 * contains custom components. Manages connections between selector and its value management
 * functions.
 * Delegate can have following signals and function to setup interaction with screen:
 *     - "applyRequest" signal to signalize that we want to apply data immediately and close
 *       selector screen.
 *     - "setValue" function to update current selected value.
 *     - "clear" function to clear currently selected value.
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
            anchors.topMargin: LayoutController.isTabletLayout ? 0 : 20
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

                topMargin: (searchEdit.visible || !LayoutController.isTabletLayout) ? 20 : 0
                leftMargin: 20
                rightMargin: 20
            }

            contentHeight: delegateLoader.height

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
