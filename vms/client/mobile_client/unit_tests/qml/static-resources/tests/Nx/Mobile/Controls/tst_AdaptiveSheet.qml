// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtTest

import Nx.Mobile.Controls
import Nx.Test
import Nx.Ui

ControlTestCase
{
    id: testCase

    name: "Nx.Mobile.Controls.AdaptiveSheet"

    component: AdaptiveSheet
    {
        Item
        {
            objectName: "spacer"

            width: parent.width
            height: 40
        }

        TextField
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

    /** Sheets are at a side edge in this layout. */
    function forceExpandedLayout()
    {
        appContext.settings.forcedLayoutMode = LayoutMode.Expanded
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

    function test_implicitSize()
    {
        // No implicit width: the sheet takes it from the parent.
        verify(control.implicitHeight > 0)
        verify(control.width > 0)
        verify(control.height > 0)
        verify(control.width <= testCase.width)
        verify(control.height <= testCase.height)
    }

    function test_defaults()
    {
        compare(control.title, "")
        compare(control.contentSpacing, 8)
        compare(control.closeAutomatically, true)
        compare(control.scrollableHeader, false)
        compare(control.scrollableFooter, false)
        compare(control.contentDragging, false)
        compare(control.preferredEdge, Qt.RightEdge)
        compare(control.footer, null)
    }

    function test_title()
    {
        control.title = "Settings"
        compare(control.title, "Settings")
        compare(control.titleTextItem.text, "Settings")
    }

    function test_titleCustomAreaFillsHeader()
    {
        control.title = "Settings"
        openControl()

        const area = control.titleCustomArea
        tryCompare(area, "x", control.titleTextItem.width + control.header.spacing)
        compare(area.x + area.width, control.header.width)
        compare(area.height, control.header.height)
    }

    function test_contentSpacing()
    {
        const spacer = findContentItem("spacer")
        const field = findContentItem("field")
        openControl()
        tryCompare(field, "y", spacer.height + 8)

        control.contentSpacing = 20
        compare(control.contentSpacing, 20)
        tryCompare(field, "y", spacer.height + 20)
    }

    function test_bottomEdgeInCompactLayout()
    {
        compare(control.edge, Qt.BottomEdge)
        compare(control.bottomEdge, true)
        compare(control.leftEdge, false)
        compare(control.rightEdge, false)

        control.preferredEdge = Qt.LeftEdge
        compare(control.edge, Qt.BottomEdge)
    }

    function test_opensAtBottom()
    {
        openControl()
        compare(control.width, testCase.width)
        compare(control.x, 0)
        tryCompare(control, "y", testCase.height - control.height)
    }

    function test_heightFollowsContent()
    {
        const spacer = findContentItem("spacer")
        const flickable = findById("flickable")
        openControl()
        verify(control.height < control.availableSheetHeight)
        compare(flickable.height, flickable.contentHeight)
        compare(flickable.interactive, false)

        const height = control.height
        spacer.height += 100
        tryCompare(control, "height", height + 100)
    }

    function test_heightIsLimitedByParent()
    {
        const flickable = findById("flickable")
        compare(control.availableSheetHeight, testCase.height)

        findContentItem("spacer").height = 1000
        openControl()
        tryCompare(control, "height", control.availableSheetHeight)
        compare(flickable.height, control.availableContentHeight)
        verify(flickable.contentHeight > flickable.height)
        compare(flickable.interactive, true)
    }

    function test_closesOnClickOutside()
    {
        openControl()
        mouseClick(testCase, testCase.width / 2, 10)
        tryCompare(control, "opened", false)
        tryCompare(control, "visible", false)
    }

    function test_closesOnEscape()
    {
        openControl()
        control.forceActiveFocus()
        verify(control.activeFocus)
        keyClick(Qt.Key_Escape)
        tryCompare(control, "opened", false)
        tryCompare(control, "visible", false)
    }

    function test_closeAutomatically()
    {
        control.closeAutomatically = false
        openControl()
        mouseClick(testCase, testCase.width / 2, 10)
        tryCompare(control, "opened", true)

        control.forceActiveFocus()
        verify(control.activeFocus)
        keyClick(Qt.Key_Escape)
        tryCompare(control, "opened", true)

        control.closeAutomatically = true
        mouseClick(testCase, testCase.width / 2, 10)
        tryCompare(control, "opened", false)
    }

    function test_closeButtonIsHiddenAtBottomEdge()
    {
        compare(control.closeButtonVisible, false)
        openControl()
        compare(findById("closeButton").visible, false)
    }

    function test_closeButtonCloses()
    {
        const closeButton = findById("closeButton")
        control.closeButtonVisible = true
        openControl()
        tryCompare(closeButton, "visible", true)

        mouseClick(closeButton)
        tryCompare(control, "opened", false)
        tryCompare(control, "visible", false)
    }

    function test_sideEdgeInExpandedLayout()
    {
        forceExpandedLayout()
        compare(control.edge, Qt.RightEdge)
        compare(control.rightEdge, true)
        compare(control.leftEdge, false)
        compare(control.bottomEdge, false)

        control.preferredEdge = Qt.LeftEdge
        compare(control.edge, Qt.LeftEdge)
        compare(control.leftEdge, true)
        compare(control.rightEdge, false)
        compare(control.bottomEdge, false)
    }

    function test_opensAtRightEdge()
    {
        forceExpandedLayout()
        openControl()
        compare(control.width, StyleHints.sheetWidth)
        compare(control.height, testCase.height)
        compare(control.availableSheetHeight, testCase.height)
        tryCompare(control, "x", testCase.width - control.width)
        compare(control.y, 0)
    }

    function test_opensAtLeftEdge()
    {
        forceExpandedLayout()
        control.preferredEdge = Qt.LeftEdge
        openControl()
        compare(control.width, StyleHints.sheetWidth)
        compare(control.height, testCase.height)
        tryCompare(control, "x", 0)
        compare(control.y, 0)
    }

    function test_sideSheetKeepsHeightWithLongContent()
    {
        const flickable = findById("flickable")
        forceExpandedLayout()
        findContentItem("spacer").height = 1000
        openControl()
        compare(control.height, testCase.height)
        compare(flickable.height, control.availableContentHeight)
        verify(flickable.contentHeight > flickable.height)
        compare(flickable.interactive, true)
    }

    function test_sideSheetCloseButton()
    {
        const closeButton = findById("closeButton")
        forceExpandedLayout()
        compare(control.closeButtonVisible, true)
        openControl()
        tryCompare(closeButton, "visible", true)

        mouseClick(closeButton)
        tryCompare(control, "opened", false)
        tryCompare(control, "visible", false)
    }

    function test_sideSheetClosesOnClickOutside()
    {
        forceExpandedLayout()
        openControl()
        tryCompare(control, "x", testCase.width - control.width)

        mouseClick(testCase, 10, testCase.height / 2)
        tryCompare(control, "opened", false)
        tryCompare(control, "visible", false)
    }

    function test_scrollableHeader()
    {
        const content = findById("content")
        const flickable = findById("flickable")
        findContentItem("spacer").height = 1000
        openControl()
        tryCompare(control, "height", control.availableSheetHeight)
        verify(flickable.y > 0)
        const headerY = control.header.mapToItem(content, 0, 0).y

        // A pinned header does not scroll.
        flickable.contentY = 50
        compare(control.header.mapToItem(content, 0, 0).y, headerY)

        // A scrollable header moves with the content.
        control.scrollableHeader = true
        tryCompare(flickable, "y", 0)
        compare(flickable.contentY, 50)
        compare(control.header.mapToItem(content, 0, 0).y, headerY - 50)
    }

    function test_footer()
    {
        const footerArea = findById("footer")
        compare(footerArea.visible, false)

        const footerItem = createTemporaryObject(footerComponent, testCase)
        verify(!!footerItem, "Component exists")
        control.footer = footerItem
        compare(control.footer, footerItem)
        openControl()

        tryCompare(footerArea, "visible", true)
        compare(footerArea.height,
            footerArea.kTopMargin + footerItem.implicitHeight + footerArea.kBottomMargin)
        compare(footerArea.parent, findById("content"))
        tryCompare(footerArea, "y", footerArea.parent.height - footerArea.height)
    }

    function test_scrollableFooter()
    {
        const footerArea = findById("footer")
        const footerItem = createTemporaryObject(footerComponent, testCase)
        verify(!!footerItem, "Component exists")
        control.footer = footerItem
        openControl()
        compare(footerArea.parent, findById("content"))

        control.scrollableFooter = true
        compare(footerArea.parent, findById("scrollableContent"))
    }

    function test_resetFocus()
    {
        const field = findContentItem("field")
        openControl()
        field.forceActiveFocus()
        verify(field.activeFocus)

        control.resetFocus()
        compare(field.activeFocus, false)
    }

    function test_clickOnEmptyAreaResetsFocus()
    {
        const field = findContentItem("field")
        openControl()
        field.forceActiveFocus()
        verify(field.activeFocus)

        mouseClick(findContentItem("spacer"))
        tryCompare(field, "activeFocus", false)
        tryCompare(control, "opened", true)
    }

    function test_focusedFieldIsScrolledIntoView()
    {
        const field = findContentItem("field")
        const flickable = findById("flickable")
        findContentItem("spacer").height = 1000
        openControl()
        tryCompare(control, "height", control.availableSheetHeight)
        compare(flickable.contentY, 0)

        field.forceActiveFocus()
        verify(field.activeFocus)
        tryCompare(flickable, "contentY", flickable.contentHeight - flickable.height)
    }

    Component
    {
        id: footerComponent

        Rectangle
        {
            implicitHeight: 40
        }
    }
}
