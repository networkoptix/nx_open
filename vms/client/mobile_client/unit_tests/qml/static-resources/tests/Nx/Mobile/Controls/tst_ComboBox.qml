// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtTest

import Nx.Mobile.Controls
import Nx.Test

ControlTestCase
{
    name: "Nx.Mobile.Controls.ComboBox"

    component: ComboBox
    {
        model: ListModel
        {
            ListElement { display: "One"; value: 1 }
            ListElement { display: "Two"; value: 2 }
            ListElement { display: "Three"; value: 3 }
        }
    }

    spiedSignals: ["accepted"]

    function openPopup()
    {
        mouseClick(control)
        tryCompare(control.popup, "opened", true)
    }

    function test_displayText()
    {
        compare(control.displayText, "One")
    }

    function test_bottomTextGrowsImplicitHeight()
    {
        compare(control.implicitHeight, 56)
        control.supportText = "Support"
        verify(control.background.bottomTextHeight > 0)
        compare(control.implicitHeight, 56 + control.background.bottomTextHeight)
    }

    function test_selectItem()
    {
        control.errorText = "Error"
        openPopup()

        const item = control.popup.contentItem.itemAtIndex(1)
        verify(!!item, "Object exists")
        compare(item.text, "Two")

        mouseClick(item)
        tryCompare(signalSpy("accepted"), "count", 1)
        tryCompare(control, "displayText", "Two")
        tryCompare(control, "errorText", "")
        tryCompare(control.popup, "visible", false)
    }

    function test_selectedItemIsMarked()
    {
        openPopup()

        const firstItem = control.popup.contentItem.itemAtIndex(0)
        verify(!!firstItem, "Object exists")
        const secondItem = control.popup.contentItem.itemAtIndex(1)
        verify(!!secondItem, "Object exists")
        compare(firstItem.selected, true)
        compare(secondItem.selected, false)
    }

    function test_cancelClosesPopup()
    {
        openPopup()

        const cancelItem = control.popup.contentItem.footerItem
        verify(!!cancelItem, "Object exists")
        compare(cancelItem.text, "Cancel")

        mouseClick(cancelItem)
        tryCompare(control.popup, "visible", false)
        tryCompare(control, "displayText", "One")
        tryCompare(signalSpy("accepted"), "count", 0)
    }

    function test_popupTitle()
    {
        control.popupTitle = "Title"
        openPopup()

        const titleItem = control.popup.contentItem.headerItem
        verify(!!titleItem, "Object exists")
        compare(titleItem.visible, true)
    }

    function test_emptyPopupTitleIsHidden()
    {
        openPopup()

        const titleItem = control.popup.contentItem.headerItem
        verify(!!titleItem, "Object exists")
        compare(titleItem.visible, false)
        compare(titleItem.height, 0)
    }

    function test_editTextSelectsMatchingItem()
    {
        control.editable = true
        control.editText = " Two "
        compare(control.currentText, "Two")
    }

    function test_unknownEditTextIsKept()
    {
        control.editable = true
        control.editText = "Four"
        compare(control.currentIndex, -1)
        compare(control.editText, "Four")
    }
}
