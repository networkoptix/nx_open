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
        Flickable
        {
            id: flickable

            width: parent.width
            height: parent.height - y
            contentWidth: optionSelectorItem.width
            contentHeight: delegateLoader.height

            Loader
            {
                id: delegateLoader

                width: optionSelectorItem.width

                sourceComponent: selector && selector.screenDelegate

                onItemChanged:
                {
                    if (!item)
                        return

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
