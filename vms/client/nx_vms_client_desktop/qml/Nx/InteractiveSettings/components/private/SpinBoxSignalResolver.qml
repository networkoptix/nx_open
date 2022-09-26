// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15
import QtQml 2.15

/**
 * Resolves SpinBox signals (valueModified, textEdited) into one signal.
 */
Item
{
    id: root

    property var target: parent
    property bool valueModified: false
    property bool isKeyboardTyping: false

    signal valueEdited(bool isKeyboardTyping)

    Connections
    {
        target: root.target

        function onValueModified() { processSignal(/*isKeyboardTyping*/ false) }
    }

    Connections
    {
        target: root.target.contentItem

        function onTextEdited() { processSignal(/*isKeyboardTyping*/ true) }
    }

    function processSignal(isKeyboardTyping)
    {
        valueModified = true
        if (isKeyboardTyping)
            root.isKeyboardTyping = true

        Qt.callLater(resolveSignals)
    }

    function resolveSignals()
    {
        if (valueModified)
            root.valueEdited(isKeyboardTyping)

        valueModified = false
        isKeyboardTyping = false
    }
}
