// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls

import Nx.Core

Flickable
{
    id: control

    property alias backgroundMode: fieldBackground.mode
    property alias labelText: fieldBackground.labelText
    property alias text: textArea.text
    property alias readOnly: textArea.readOnly

    implicitWidth: 268
    implicitHeight: Math.min(150, textArea.implicitHeight)

    boundsBehavior: Flickable.StopAtBounds
    flickableDirection: Flickable.VerticalFlick

    TextArea.flickable: TextArea
    {
        id: textArea

        wrapMode: TextArea.Wrap

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
            const textAreaPos = textArea.mapToItem(control.contentItem, 0, 0)

            var minY = textAreaPos.y
                + cursorRectangle.y
                + cursorRectangle.height
                + textArea.bottomPadding
                - control.height
            var maxY = textAreaPos.y
                + cursorRectangle.y
                - textArea.topPadding

            if (control.contentY >= maxY) //< Cursor is above top position.
                control.contentY = maxY
            else if (control.contentY <= minY) // Cursor is below bottom position.
                control.contentY = minY
        }
    }

    // Update bounds to avoid QTBUG-60296 when initial position of text is wrong and shifted
    // for the TextArea paddings twice.
    Component.onCompleted: returnToBounds()
}
