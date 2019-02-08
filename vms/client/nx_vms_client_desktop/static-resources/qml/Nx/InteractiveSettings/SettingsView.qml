import QtQuick 2.0

import "settings.js" as Settings
import "components"

Item
{
    id: settingsView

    signal valuesChanged()
    signal valuesEdited()

    property Item contentItem: null

    function loadModel(model, initialValues)
    {
        if (contentItem)
            contentItem.destroy()

        contentItem = Settings.createItems(settingsView, model)

        if (initialValues)
            setValues(initialValues)
    }

    function getValues()
    {
        return contentItem && Settings.getValues(contentItem)
    }

    function setValues(values)
    {
        if (!contentItem)
            return

        _valuesChangedEnabled = false
        Settings.setValues(contentItem, values)
        valuesChanged()
        _valuesChangedEnabled = true
    }

    property bool _valuesChangedEnabled
    function _emitValuesChanged()
    {
        if (_valuesChangedEnabled)
            valuesChanged()
    }

    function triggerValuesEdited()
    {
        if (!_valuesChangedEnabled)
            return

        valuesEdited()
        valuesChanged()
    }
}
