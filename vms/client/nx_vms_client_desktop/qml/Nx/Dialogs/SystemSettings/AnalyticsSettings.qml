// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.11
import QtQuick.Layouts 1.11
import Nx 1.0
import Nx.Items 1.0
import Nx.Utils 1.0
import Nx.Controls 1.0
import Nx.Controls.NavigationMenu 1.0
import Nx.InteractiveSettings 1.0

Item
{
    id: analyticsSettings

    property var store
    readonly property alias currentEngineId: menu.currentEngineId
    property var engines: store ? store.analyticsEngines : []
    property var currentEngineInfo: engines.find(engine => engine.id == currentEngineId)

    AnalyticsSettingsMenu
    {
        id: menu

        width: 240
        height: parent.height

        engines: analyticsSettings.engines
        enabledEngines: engines.map(engine => engine.id)

        onCurrentEngineIdChanged:
            activateEngine(currentEngineId)
        onCurrentSectionPathChanged:
            settingsView.selectSection(currentSectionPath)
    }

    Item
    {
        id: scrollBarParent

        width: 8

        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
    }

    SettingsView
    {
        id: settingsView

        scrollBarParent: scrollBarParent

        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: menu.right
        anchors.right: scrollBarParent.left
        anchors.margins: 16

        headerItem: InformationPanel
        {
            checkable: false
            refreshable: false
            streamSelectorVisible: false
            engineInfo: currentEngineInfo
        }

        placeholderItem: SettingsPlaceholder
        {
            header: qsTr("This integration has no settings at System level.")
            description: qsTr("Check Camera Settings to configure this integration.")
        }

        onValuesEdited: function(activeItem)
        {
            store.setSettingsValues(
                currentEngineId,
                activeItem ? activeItem.name : "",
                activeItem ? activeItem.parametersModel : "",
                getValues())
        }
    }

    Connections
    {
        target: analyticsSettings.store || null

        function onSettingsValuesChanged(engineId)
        {
            if (engineId === currentEngineId)
                settingsView.setValues(store.settingsValues(engineId))
        }

        function onSettingsModelChanged(engineId)
        {
            if (engineId === currentEngineId)
                updateModel(engineId)
        }

        function onErrorsChanged(engineId)
        {
            settingsView.setErrors(store.errors(engineId))
        }

        function onAnalyticsEnginesChanged()
        {
            for (var i = 0; i < store.analyticsEngines.length; ++i)
            {
                if (store.analyticsEngines[i].id === currentEngineId)
                    return
            }

            if (store.analyticsEngines.length > 0)
                activateEngine(store.analyticsEngines[0].id)
            else
                activateEngine(null)
        }
    }

    function activateEngine(engineId)
    {
        if (currentEngineId !== engineId)
        {
            menu.currentEngineId = engineId
            menu.currentSectionPath = []
            menu.currentItemId = engineId ? (engineId + "|") : ""
        }

        updateModel(engineId)
    }

    function updateModel(engineId)
    {
        const model = engineId ? store.settingsModel(engineId) : {}
        const values = engineId ? store.settingsValues(engineId) : {}
        settingsView.loadModel(model, values, /*restoreScrollPosition*/ true)
        settingsView.selectSection(menu.currentSectionPath)
        menu.currentEngineSettingsModel = model
    }

    onCurrentEngineIdChanged:
    {
        // Workaround for META-183.
        // TODO: #dklychkov Fix properly.
        // For some reason the notify signal of currentEngineId is not caught by C++ for the very
        // first time. However any handler on QML side fixes it. This is why this empty handler is
        // here.
    }
}
