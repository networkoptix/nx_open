import QtQuick 2.0
import QtQuick.Controls 2.4

import "settings.js" as Settings
import "components"

Item
{
    id: settingsView

    signal valuesChanged()
    signal valuesEdited()

    property Item contentItem: null
    property bool contentEnabled: true
    property Item scrollBarParent: null
    property Item headerItem: null

    property var resourceId
    property var engineId

    function loadModel(model, initialValues)
    {
        if (contentItem)
            contentItem.destroy()

        contentItem = Settings.createItems(settingsView, model)
        contentItem.contentEnabled = Qt.binding(function() { return contentEnabled })
        contentItem.scrollBarParent = Qt.binding(function() { return scrollBarParent })
        contentItem.extraHeaderItem = Qt.binding(function() { return headerItem })

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

    property bool _valuesChangedEnabled: true
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
