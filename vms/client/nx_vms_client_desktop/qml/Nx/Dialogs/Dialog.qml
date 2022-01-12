// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Window 2.14

import Nx 1.0
import Nx.Controls 1.0

import nx.client.desktop 1.0

Window
{
    id: dialog

    signal accepted()
    signal rejected()
    signal applied()

    property DialogButtonBox buttonBox: null
    property Item contentItem: null
    property list<Item> customButtons

    property real leftPadding: 16
    property real rightPadding: 16
    property real topPadding: 16
    property real bottomPadding: 16

    readonly property real availableWidth: width - leftPadding - rightPadding
    readonly property real availableHeight:
        height - topPadding - bottomPadding - (buttonBox ? buttonBox.height : 0)

    // Dig out the shadowed contentItem of Window.
    readonly property alias rootItem: dummy.parent
    Item { id: dummy; visible: false }

    flags: Qt.Dialog
    color: ColorTheme.colors.dark7

    property var result: undefined

    Instrument
    {
        item: rootItem
        onKeyPress: event =>
        {
            switch (event.key)
            {
                case Qt.Key_Escape:
                    reject()
                    return
                case Qt.Key_Return:
                case Qt.Key_Enter:
                    accept()
                    return
            }
        }
    }

    onContentItemChanged:
    {
        if (!contentItem)
            return

        contentItem.parent = rootItem
        contentItem.x = Qt.binding(() => leftPadding)
        contentItem.y = Qt.binding(() => topPadding)
        contentItem.width = Qt.binding(() => availableWidth)
        contentItem.height = Qt.binding(() => availableHeight)
        contentItem.focus = true
    }

    onButtonBoxChanged:
    {
        if (!buttonBox)
            return

        buttonBox.parent = rootItem
        buttonBox.accepted.connect(dialog.accept)
        buttonBox.rejected.connect(dialog.reject)
        buttonBox.applied.connect(dialog.apply)
    }

    onCustomButtonsChanged:
    {
        for (let i = 0; i < customButtons.length; ++i)
            customButtons[i].parent = buttonBox.contentItem
    }

    onVisibilityChanged:
    {
        if (visible)
        {
            result = undefined
            return
        }

        if (result === undefined)
        {
            result = false
            dialog.rejected()
            // Do not close the dialog here - it won't open again.
        }
    }

    function accept()
    {
        result = true
        dialog.accepted()
        close()
    }

    function reject()
    {
        result = false
        dialog.rejected()
        close()
    }

    function apply()
    {
        dialog.applied()
    }
}
