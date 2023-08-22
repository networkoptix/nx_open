// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx

import "settings.js" as Settings

/**
 * Analytics Settings View Model represents common view state and logic of Analytics Settings
 * (AnalyticsSettingsView and AnalyticsSettingsMenu).
 * When an engine is requested (see engineRequested) the underlying model must call updateState()
 * method with parameters corresponding to the requested engine.
 */
NxObject
{
    id: model

    /** Emitted when current engine id is changed (for example, on click or model reset). */
    signal engineRequested(var engineId)

    // Common state properties.
    readonly property alias engines: impl.engines
    readonly property alias currentEngineSettingsModel: impl.currentEngineSettingsModel
    readonly property alias currentSettingsErrors: impl.currentSettingsErrors
    readonly property alias currentEngineLicenseSummary: impl.currentEngineLicenseSummary
    property var enabledEngines: engines.map(engine => engine.id) //< Enabled by default.
    property var requestsModel

    readonly property var currentEngineInfo: findEngineInfo(currentEngineId)
    readonly property var requests: requestsModel ? requestsModel.requests : null

    // Menu related properties.
    readonly property alias currentEngineId: impl.currentEngineId
    readonly property alias currentItemType: impl.currentItemType
    readonly property alias currentSectionId: impl.currentSectionId
    readonly property var currentItem:
        ({ type: currentItemType, engineId: currentEngineId, sectionId: currentSectionId })

    readonly property var sections: currentEngineSettingsModel
        ? Settings.buildSectionPaths(currentEngineSettingsModel)
        : {}

    readonly property var currentSectionPath: sections[currentSectionId] ?? []

    // Settings View state grouped signals as SettingsView supports only grouped updates.
    signal settingsViewStateChanged(var model, var values)
    signal settingsViewValuesChanged(var values, bool isInitial)

    NxObject
    {
        id: impl

        // Common state properties.
        property var engines: []
        property var currentEngineSettingsModel
        property var currentSettingsErrors
        property var currentEngineLicenseSummary

        // Menu related properties.
        property var currentEngineId
        property var currentItemType
        property var currentSectionId
    }

    Component.onCompleted: switchToDefaultState()

    function switchToDefaultState()
    {
        setCurrentItem("Plugins", /*engineId*/ null, /*sectionId*/ null)
        updateModel(/*model*/ null, /*initialValues*/ null)
    }

    function updateState(engines, licenseSummary, engineId, model, values, errors, isInitial)
    {
        if (JSON.stringify(impl.engines) !== JSON.stringify(engines))
            impl.engines = engines

        impl.currentEngineLicenseSummary = licenseSummary

        if (!engineId)
            return

        if (findEngineInfo(engineId))
        {
            impl.currentEngineId = engineId
            impl.currentItemType = "Engine"

            if (typeof(model) !== "undefined")
                updateModel(model, values, isInitial)

            impl.currentSettingsErrors = errors
        }
        else if (currentEngineId)
        {
            switchToDefaultState()
        }
    }

    function updateModel(model, values, isInitial)
    {
        const actualModel = model ? preprocessedModel(model) : null
        if (JSON.stringify(currentEngineSettingsModel) === JSON.stringify(actualModel))
        {
            settingsViewValuesChanged(values, isInitial)
        }
        else
        {
            impl.currentEngineSettingsModel = actualModel

            if (!(currentSectionId in sections))
                impl.currentSectionId = null

            settingsViewStateChanged(currentEngineSettingsModel, values)
        }
    }

    function setCurrentItem(type, engineId, sectionId)
    {
        if (type !== "Engine")
            impl.currentItemType = type

        impl.currentSectionId = sectionId

        if (currentEngineId !== engineId)
        {
            if (currentItemType !== "Engine" && !engineId)
                impl.currentEngineId = null

            // Keep the underlying model in the actual state even if the engine id is null.
            requestEngine(engineId)
        }
    }

    /**
     * Sets current engine (engineRequested is emitted).
     */
    function setCurrentEngine(engineId)
    {
        setCurrentItem("Engine", engineId, /*sectionId*/ null)
    }

    function requestEngine(engineId)
    {
        viewModel.engineRequested(engineId)
    }

    function findEngineInfo(engineId)
    {
        return engines.find(engine => engine.id === engineId)
    }

    function preprocessedModel(model)
    {
        let modelCopy = JSON.parse(JSON.stringify(model))
        Settings.preprocessModel(modelCopy)
        return modelCopy
    }
}
