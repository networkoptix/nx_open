import QtQuick 2.5

QtObject
{
    id: thisComponent;

    property bool isSomeoneActive: (currentItem !== null);
    property string variableName;
    property string writeVariableName: variableName;
    property string signalName: variableToSignalName(variableName);

    // Impl properties
    property QtObject currentItem;

    function addItem(item)
    {
        var handler = function()
        {
            setCurrentItem(item);
        }

        item[signalName].connect(handler);
    }

    function setCurrentItem(item)
    {
        if (!item || item[variableName])
        {
            if (currentItem)
                currentItem[writeVariableName] = false;

            currentItem = item;
        }
        else
            currentItem = null;
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

