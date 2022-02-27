// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQml 2.15

import Qt.labs.platform 1.1

MenuItem
{
    id: item

    property Action action

    onActionChanged:
    {
        if (action)
        {
            checkable = Qt.binding(() => action.checkable)
            checked = Qt.binding(() => action.checked)
            enabled = Qt.binding(() => action.enabled)
            icon.source = Qt.binding(() => action.icon.source)
            icon.name = Qt.binding(() => action.icon.name)
            text = Qt.binding(() => action.text.replace(/&/g, "&&"))
            shortcut = Qt.binding(() => action.shortcut)
            visible = Qt.binding(() => action.visible)
        }
        else
        {
            checkable = false
            checked = false
            enabled = true
            icon.source = ""
            icon.name = ""
            text = ""
            shortcut = ""
            visible = true
        }
    }

    onTriggered:
    {
        if (action)
            action.trigger(item)
    }
}
