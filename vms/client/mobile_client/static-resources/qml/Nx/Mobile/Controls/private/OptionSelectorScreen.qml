// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15

import Nx.Core 1.0
import Nx.Controls 1.0
import Nx.Mobile.Controls 1.0
import Nx.Ui 1.0

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
Page
{
    id: screen

    property alias clearButtonText: clearButton.text
    property alias delegateItem: delegateLoader.item
    property OptionSelector selector: null

    function apply()
    {
        d.callDelegateFunction("apply")
        Workflow.popCurrentScreen()
    }

    contentItem: FocusScope
    {
        Flickable
        {
            id: flickable

            y: 16
            width: parent.width
            height: parent.height - y
            contentWidth: screen.width
            contentHeight: delegateLoader.height

            Loader
            {
                id: delegateLoader

                width: screen.width

                sourceComponent: selector && selector.screenDelegate
                onStatusChanged:
                {
                    if (status !== Loader.Ready)
                        return

                    if (item.hasOwnProperty("selector"))
                        item.selector = screen.selector
                    if (item.hasOwnProperty("setValue"))
                        item.setValue(selector.value)
                }
            }
        }
    }

    customBackHandler: () => apply()
    onLeftButtonClicked: apply()

    titleControls:
    [
        TextButton
        {
            id: clearButton

            text: qsTr("Clear")
            visible: screen.selector && !screen.selector.isDefaultValue

            onClicked: d.callDelegateFunction("clear")
        }
    ]

    Component.onCompleted:
    {
        if (delegateLoader.item && delegateLoader.item.hasOwnProperty("applyRequested"))
            delegateLoader.item.applyRequested.connect(apply)
    }

    Component.onDestruction:
    {
        if (delegateLoader.item && delegateLoader.item.hasOwnProperty("applyRequested"))
            delegateLoader.item.applyRequested.disconnect(apply)
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
