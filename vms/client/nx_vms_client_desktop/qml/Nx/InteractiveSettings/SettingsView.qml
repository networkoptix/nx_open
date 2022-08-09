// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.0
import QtQuick.Controls 2.4

import Nx 1.0

import "settings.js" as Settings
import "components"

Item
{
    id: settingsView

    signal valuesChanged()
    signal valuesEdited(Item activeItem)

    property Item contentItem: null
    property bool contentEnabled: true
    property bool contentVisible: true
    property Item scrollBarParent: null
    property Item headerItem: null
    property Item placeholderItem: null

    property var resourceId: NxGlobals.uuid("")
    property var engineId: NxGlobals.uuid("")

    implicitWidth: contentItem ? contentItem.implicitWidth : 0
    implicitHeight: contentItem ? contentItem.implicitHeight : 0

    QtObject
    {
        id: impl
        property var sectionPaths: ({})
        property bool valuesChangedEnabled: true
    }

    function loadModel(model, initialValues, restoreScrollPosition)
    {
        let scrollPosition = contentItem ? contentItem.verticalScrollBar.position : 0.0

        if (contentItem)
        {
            contentItem.visible = false
            contentItem.destroy()
        }

        contentItem = Settings.createItems(settingsView, model)
        if (!contentItem)
            return

        contentItem.contentEnabled = Qt.binding(function() { return contentEnabled })
        contentItem.contentVisible = Qt.binding(function() { return contentVisible })
        contentItem.scrollBarParent = Qt.binding(function() { return scrollBarParent })
        contentItem.extraHeaderItem = Qt.binding(function() { return headerItem })
        contentItem.placeholderItem = Qt.binding(function() { return placeholderItem })
        impl.sectionPaths = Settings.buildSectionPaths(model)

        if (restoreScrollPosition)
        {
            let scroll =
                () =>
                {
                    contentItem.verticalScrollBar.sizeChanged.disconnect(scroll)
                    contentItem.verticalScrollBar.position = scrollPosition
                }

            // Scroll after resize.
            contentItem.verticalScrollBar.sizeChanged.connect(scroll)
        }

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

        impl.valuesChangedEnabled = false
        Settings.setValues(contentItem, values)
        valuesChanged()
        impl.valuesChangedEnabled = true
    }

    function _emitValuesChanged()
    {
        if (impl.valuesChangedEnabled)
            valuesChanged()
    }

    function triggerValuesEdited(activeItem)
    {
        if (!impl.valuesChangedEnabled)
            return

        valuesEdited(activeItem)
        valuesChanged()
    }

    function selectSection(sectionPath)
    {
        let stack = contentItem && contentItem.sectionStack
        if (!stack)
            return

        for (let i = 0; i < sectionPath.length; ++i)
        {
            var sectionIndex = sectionPath[i] + 1
            if (sectionIndex < stack.count)
            {
                stack.currentIndex = sectionIndex
                stack = stack.children[sectionIndex].sectionStack
            }
        }

        if (stack)
            stack.currentIndex = 0
    }

    function sectionName(model, sectionPath)
    {
        return Settings.sectionNameByPath(model, sectionPath)
    }

    function sectionPath(sectionName)
    {
        return impl.sectionPaths[sectionName] ?? []
    }

    function preprocessedModel(model)
    {
        let modelCopy = JSON.parse(JSON.stringify(model))
        Settings.preprocessModel(modelCopy)
        return modelCopy
    }

    function setErrors(errors)
    {
        Settings.setErrors(contentItem, errors)
    }
}
