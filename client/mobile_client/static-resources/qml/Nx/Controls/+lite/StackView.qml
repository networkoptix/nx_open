import QtQuick 2.6
import QtQuick.Controls 2.4

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
