// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15
import QtQml 2.15

/**
 * Provides Active Settings behavior for TextField like items. It defines the behavior of the
 * activeValueEdited signal depending on signals (valueEdited, activeValueEdited) and isActive
 * property of the target.
 */
Item
{
    id: root

    /** Target to add Active Settings behavior. */
    property var target: parent

    /** Delay for emitting activeValueEdited signal after the valueEdited signal. */
    property int activeValueDelayMs: 2000

    Connections
    {
        target: root.target
        enabled: root.target.isActive

        // Process the valueEdited signal last (to be sure that the activeValueEdited signal is
        // going to be emitted).
        function onValueEdited() { Qt.callLater(activeValueDelayedSignal.restart) }
        function onValueChanged() { activeValueDelayedSignal.stop() }
        function onActiveValueEdited() { activeValueDelayedSignal.stop() }
    }

    Timer
    {
        id: activeValueDelayedSignal
        interval: activeValueDelayMs
        onTriggered:
        {
            if (target.isActive)
                target.activeValueEdited()
        }
    }
}
