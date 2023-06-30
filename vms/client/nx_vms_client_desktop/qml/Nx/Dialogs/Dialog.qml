// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Window

import Nx.Core
import Nx.Controls

import nx.vms.client.desktop

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

    property real implicitWidth: leftPadding + rightPadding
	+ Math.max(contentItem ? contentItem.implicitWidth : 0,
            buttonBox ? buttonBox.implicitWidth : 0)

    property real implicitHeight: topPadding + bottomPadding
        + (contentItem ? contentItem.implicitHeight : 0)
        + (buttonBox ? buttonBox.implicitHeight : 0)

    property real fixedWidth: 0
    property real fixedHeight: 0

    // Dig out the shadowed contentItem of Window.
    readonly property alias rootItem: dummy.parent
    Item { id: dummy; visible: false }

    flags: Qt.Dialog
    color: ColorTheme.colors.dark7

    property var result: undefined

    property bool enabled: true //< Required for GUI autotesting.

    Shortcut
    {
        sequence: "Esc"
        onActivated: dialog.reject()
    }

    Shortcut
    {
        sequences: ["Enter", "Return"]
        onActivated: dialog.accept()
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
        contentItem.enabled = Qt.binding(() => enabled)
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

    Binding
    {
        target: dialog
        when: dialog.fixedWidth > 0
        property: "minimumWidth"
        value: dialog.fixedWidth
    }

    Binding
    {
        target: dialog
        when: dialog.fixedWidth > 0
        property: "maximumWidth"
        value: dialog.fixedWidth
    }

    Binding
    {
        target: dialog
        when: dialog.fixedHeight > 0
        property: "minimumHeight"
        value: dialog.fixedHeight
    }

    Binding
    {
        target: dialog
        when: dialog.fixedHeight > 0
        property: "maximumHeight"
        value: dialog.fixedHeight
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
