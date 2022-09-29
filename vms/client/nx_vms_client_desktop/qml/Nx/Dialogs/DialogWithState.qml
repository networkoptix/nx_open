// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15

import Nx.Dialogs 1.0

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
}
