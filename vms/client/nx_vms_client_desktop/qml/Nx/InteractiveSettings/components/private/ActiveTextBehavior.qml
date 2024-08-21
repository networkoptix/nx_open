// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15
import QtQml 2.15

/**
 * Provides Active Settings behavior for TextField like items. It defines the behavior of the
 * activated signal depending on signals (valueEdited, editingFinished) and isActive property of
 * the target.
 */
Item
{
    id: root

    /** Target to add Active Settings behavior. */
    property var target: parent

    /** Delay of the activated signal after the valueEdited signal. */
    property int activationDelayMs: 2000

    property bool activateOnFinished: false

    Connections
    {
        target: root.target
        enabled: root.target.isActive

        // Process the valueEdited signal last (to be sure that the activated signal is going to be
        // emitted).
        function onValueEdited(activated)
        {
            activateOnFinished = !activated

            if (activated)
                activatedSignal.stop()
            else
                Qt.callLater(activatedSignal.restart)
        }

        function onValueChanged() { activatedSignal.stop() }

        function onEditingFinished()
        {
            if (activateOnFinished)
                target.valueEdited(/*activated*/ true)

            activateOnFinished = false
        }
    }

    Timer
    {
        id: activatedSignal
        interval: activationDelayMs

        onTriggered:
        {
            if (target.isActive)
                target.valueEdited(/*activated*/ true)
        }
    }
}
