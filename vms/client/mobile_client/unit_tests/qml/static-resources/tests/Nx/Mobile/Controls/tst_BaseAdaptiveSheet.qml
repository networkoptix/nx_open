// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtTest

import Nx.Mobile.Controls
import Nx.Test
import Nx.Ui

ControlTestCase
{
    id: testCase

    name: "Nx.Mobile.Controls.BaseAdaptiveSheet"

    component: BaseAdaptiveSheet
    {
        Rectangle
        {
            objectName: "sheetContent"

            width: parent.width
            height: 100
        }

        TextInput
        {
            objectName: "field"

            width: parent.width
        }
    }

    property int initialLayoutMode

    function initTestCase()
    {
        initialLayoutMode = appContext.settings.forcedLayoutMode
    }

    function cleanup()
    {
        appContext.settings.forcedLayoutMode = initialLayoutMode
    }

    function openControl()
    {
        control.open()
        tryCompare(control, "opened", true)
    }

    function findContentItem(name)
    {
        const result = findChild(control.contentItem, name)
        verify(!!result, "Object exists")
        return result
    }

    function createEdgeItem()
    {
        const result = createTemporaryObject(edgeItemComponent, testCase)
        verify(!!result, "Component exists")
        return result
    }

    /** The sheet has no implicit width: its size is set by the layout of the window. */
    function test_implicitSize()
    {
        compare(control.width, testCase.width)
        verify(control.height > 0)
        verify(control.height <= testCase.height)
    }

    function test_defaults()
    {
        compare(control.header, null)
        compare(control.footer, null)
        compare(control.closeAutomatically, true)
        compare(control.preferredEdge, Qt.RightEdge)
        compare(control.scrollableHeader, false)
        compare(control.scrollableFooter, false)
        compare(control.contentDragging, false)
    }

    function test_bottomEdgeInCompactLayout()
    {
        compare(control.edge, Qt.BottomEdge)
        compare(control.bottomEdge, true)
        compare(control.leftEdge, false)
        compare(control.rightEdge, false)
        compare(control.closeButtonVisible, false)
        compare(control.availableSheetHeight, testCase.height)
    }

    function test_sideEdgeInExpandedLayout()
    {
        appContext.settings.forcedLayoutMode = LayoutMode.Expanded
        compare(control.edge, Qt.RightEdge)
        compare(control.rightEdge, true)
        compare(control.bottomEdge, false)
        compare(control.closeButtonVisible, true)
        compare(control.width, StyleHints.sheetWidth)
        compare(control.height, testCase.height)

        control.preferredEdge = Qt.LeftEdge
        compare(control.edge, Qt.LeftEdge)
        compare(control.leftEdge, true)
        compare(control.rightEdge, false)
    }

    function test_closePolicy()
    {
        compare(control.closePolicy, Popup.CloseOnEscape | Popup.CloseOnReleaseOutside)
        control.closeAutomatically = false
        compare(control.closePolicy, Popup.NoAutoClose)
    }

    function test_openClose()
    {
        openControl()
        compare(control.visible, true)

        control.close()
        tryCompare(control, "visible", false)
    }

    function test_contentIsInColumn()
    {
        openControl()

        const contentColumn = findById("contentColumn")
        const sheetContent = findContentItem("sheetContent")
        compare(sheetContent.parent, contentColumn)
        compare(findContentItem("field").parent, contentColumn)
        compare(sheetContent.width, control.width - 48)
    }

    function test_heightFollowsContent()
    {
        openControl()

        const sheetContent = findContentItem("sheetContent")
        const initialHeight = control.height
        sheetContent.height = 200
        tryCompare(control, "height", initialHeight + 100)
        compare(findById("flickable").interactive, false)
    }

    function test_heightIsLimitedByWindow()
    {
        openControl()

        findContentItem("sheetContent").height = 1000
        tryCompare(control, "height", control.availableSheetHeight)
        compare(findById("flickable").interactive, true)
    }

    function test_pinnedHeaderTakesContentSpace()
    {
        const initialHeight = control.availableContentHeight
        const headerItem = createEdgeItem()
        control.header = headerItem

        const pinnedHeight = control.availableContentHeight
        verify(pinnedHeight < initialHeight)

        control.scrollableHeader = true
        verify(control.availableContentHeight > pinnedHeight)
    }

    function test_pinnedFooterTakesContentSpace()
    {
        const initialHeight = control.availableContentHeight
        const footerItem = createEdgeItem()
        control.footer = footerItem

        const pinnedHeight = control.availableContentHeight
        verify(pinnedHeight < initialHeight)

        control.scrollableFooter = true
        verify(control.availableContentHeight > pinnedHeight)
    }

    function test_closeButton()
    {
        control.closeButtonVisible = true
        openControl()
        mouseClick(findById("closeButton"))
        tryCompare(control, "visible", false)
    }

    function test_escapeCloses()
    {
        openControl()
        control.forceActiveFocus()
        verify(control.activeFocus)
        keyClick(Qt.Key_Escape)
        tryCompare(control, "visible", false)
    }

    function test_clickOutsideCloses()
    {
        openControl()
        mouseClick(testCase, 10, 10)
        tryCompare(control, "visible", false)
    }

    function test_clickOutsideDoesNotCloseWithoutCloseAutomatically()
    {
        control.closeAutomatically = false
        openControl()
        mouseClick(testCase, 10, 10)
        tryCompare(control, "opened", true)
    }

    function test_resetFocus()
    {
        openControl()

        const field = findContentItem("field")
        field.forceActiveFocus()
        verify(field.activeFocus)

        control.resetFocus()
        compare(field.activeFocus, false)
    }

    function test_clickOnContentResetsFocus()
    {
        openControl()

        const field = findContentItem("field")
        field.forceActiveFocus()
        verify(field.activeFocus)

        mouseClick(findContentItem("sheetContent"))
        tryCompare(field, "activeFocus", false)
    }

    Component
    {
        id: edgeItemComponent

        Rectangle
        {
            implicitWidth: 100
            implicitHeight: 40
        }
    }
}
