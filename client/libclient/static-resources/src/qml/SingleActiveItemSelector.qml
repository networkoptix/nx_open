import QtQuick 2.5

QtObject
{
    id: thisComponent;

    property bool isSomeoneActive: (currentItem !== null);
    property string variableName;
    property var deactivateFunc;

    // Impl properties
    property QtObject currentItem;

    function addItem(item)
    {
        var handler = function()
        {
            setCurrentItem(item);
        }

        var signalName = variableToSignalName(variableName);
        item[signalName].connect(handler);
    }

    function setCurrentItem(item)
    {
        if (!item || item[variableName])
        {
            if (currentItem && deactivateFunc)
                deactivateFunc(currentItem);

            currentItem = item;
        }
        else if (currentItem == item)
        {
            currentItem = null;
        }
    }

    function resetCurrentItem()
    {
        setCurrentItem(null);
    }

    function variableToSignalName(variable)
    {
        return "on" + variable.charAt(0).toUpperCase()
                + variable.slice(1) + "Changed";
    }
}

