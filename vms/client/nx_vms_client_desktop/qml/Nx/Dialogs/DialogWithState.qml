// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15

import Nx.Dialogs 1.0

import "../Controls"

Dialog
{
    id: dialog

    // Automaticaly modified property required by QmlDialogWithState.
    property bool modified: false

    property var validateFunc: () => true

    signal stateAccepted()
    signal stateApplied()

    function accept()
    {
        if (validateFunc())
            stateAccepted() //< Delay accepted() signal until state is successfully saved.
    }

    function apply()
    {
        if (validateFunc())
            stateApplied()
    }

    function findAndFocusTextWithWarning(item)
    {
        if (!item.visible)
            return false

        if (item instanceof TextFieldWithWarning)
        {
            if (!item.warningState)
                return false

            recursiveForceActiveFocus(item)
            return true
        }
        return Array.prototype.find.call(item.children,
            child => findAndFocusTextWithWarning(child))
    }

    function recursiveForceActiveFocus(item)
    {
        if (item)
        {
            recursiveForceActiveFocus(item.parent)
            item.forceActiveFocus()
        }
    }
}
