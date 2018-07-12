import QtQuick 2.6
import Qt.labs.controls 1.0

StackView
{
    id: stackView
    function setScaleTransitionHint(xHint, yHint) {}

    function safePush(url, properties, operation)
    {
        return push(url, properties, operation)
    }

    function safeReplace(target, item, properties, operation)
    {
        return replace(target, item, properties, operation)
    }

    onBusyChanged:
    {
        if (!busy && currentItem)
            currentItem.forceActiveFocus()
    }

    replaceEnter: null
    replaceExit: null
    pushEnter: null
    pushExit: null
    popEnter: null
    popExit: null
}
