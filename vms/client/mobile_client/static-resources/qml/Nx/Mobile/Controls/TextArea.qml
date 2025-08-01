// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls

import Nx.Core

import nx.vms.client.mobile

import "private"

Item
{
    id: control

    property alias backgroundMode: fieldBackground.mode
    property alias labelText: fieldBackground.labelText
    property alias text: textArea.text
    property alias readOnly: textArea.readOnly
    property bool customSelection: Qt.platform.os == "android"

    implicitWidth: 268
    implicitHeight: Math.min(150, textArea.implicitHeight)

    Flickable
    {
        id: flickable

        anchors.fill: parent

        boundsBehavior: Flickable.StopAtBounds
        flickableDirection: Flickable.VerticalFlick

        TextArea.flickable: TextArea
        {
            id: textArea

            wrapMode: TextArea.Wrap
            persistentSelection: customSelection
            selectByMouse: !customSelection
            selectByKeyboard: !customSelection

            topPadding: 30
            leftPadding: 12
            rightPadding: 12

            font.pixelSize: 16
            font.weight: 500
            color: enabled
                ? ColorTheme.colors.light4
                : ColorTheme.transparent(ColorTheme.colors.light4, 0.3)

            background: FieldBackground
            {
                id: fieldBackground

                owner: textArea
            }

            focus: false

            // Workaround for QTBUG-74055 - track cursor position correctly when we have
            // top/bottom paddings set.
            onCursorRectangleChanged:
            {
                // Maps position of root item in coordinates of flickable's contentItem.
                const textAreaPos = textArea.mapToItem(flickable.contentItem, 0, 0)

                var minY = textAreaPos.y
                    + cursorRectangle.y
                    + cursorRectangle.height
                    + textArea.bottomPadding
                    - flickable.height
                var maxY = textAreaPos.y
                    + cursorRectangle.y
                    - textArea.topPadding

                if (flickable.contentY >= maxY) //< Cursor is above top position.
                    flickable.contentY = maxY
                else if (flickable.contentY <= minY) // Cursor is below bottom position.
                    flickable.contentY = minY
            }

            Component.onCompleted: TextInputWorkaround.setup(textArea)
        }

        SmoothedAnimation on contentY
        {
            id: scrollingAnimation
        }

        function scrollTo(contentY)
        {
            scrollingAnimation.to = contentY
            scrollingAnimation.restart()
        }

        function scrollToTextPosition(position)
        {
            const rect = textArea.positionToRectangle(position)

            if (rect.top - textArea.topPadding < flickable.contentY)
                flickable.scrollTo(rect.top - textArea.topPadding)
            else if (rect.bottom - flickable.height + textArea.bottomPadding > flickable.contentY)
                flickable.scrollTo(rect.bottom - flickable.height + textArea.bottomPadding)
        }

        // Update bounds to avoid QTBUG-60296 when initial position of text is wrong and shifted
        // for the TextArea paddings twice.
        Component.onCompleted: returnToBounds()
    }

    function selectText(start, end)
    {
        let focusPosition = null
        if (textArea.selectionStart != start)
            focusPosition = start
        if (textArea.selectionEnd != end)
            focusPosition = end

        // Keep the current scroll position during selection, since scrolling is implemented using
        // animation.
        const currentY = flickable.contentY
        textArea.select(start, end)
        flickable.contentY = currentY

        if (focusPosition)
            flickable.scrollToTextPosition(focusPosition)
    }

    function selectWord(position)
    {
        // Keep the current scroll position during selection, since scrolling is implemented using
        // animation.
        const currentY = flickable.contentY
        textArea.cursorPosition = position
        textArea.selectWord()
        flickable.contentY = currentY

        flickable.scrollToTextPosition(position)
    }

    Loader
    {
        id: textSelectionArea

        active: customSelection && textArea.activeFocus

        anchors.fill: parent
        anchors.topMargin: textArea.topPadding

        sourceComponent: Component
        {
            TextSelectionArea
            {
                target: textArea
                selectText: control.selectText
                selectWord: control.selectWord
                menuEnabled: !flickable.movingVertically
            }
        }
    }
}
