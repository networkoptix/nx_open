// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx

import nx.vms.client.desktop

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

    readonly property var currentEngineInfo:
        findEngineInfo(currentEngineId) || engineInfoFromRequest(currentRequest)

    readonly property alias requests: impl.requests
    readonly property alias currentRequestId: impl.currentRequestId
    readonly property var currentRequest: findRequest(currentRequestId)

    // Menu related properties.
    readonly property alias currentEngineId: impl.currentEngineId
    readonly property alias currentItemType: impl.currentItemType
    readonly property alias currentSectionId: impl.currentSectionId
    readonly property var currentItem: ({
        type: currentItemType,
        engineId: currentEngineId,
        sectionId: currentSectionId,
        requestId: currentRequestId})

    readonly property var sections: currentEngineSettingsModel
        ? Settings.buildSectionPaths(currentEngineSettingsModel)
        : {}

    readonly property var currentSectionPath: sections[currentSectionId] ?? []

    // Settings View state grouped signals as SettingsView supports only grouped updates.
    signal settingsViewStateChanged(var model, var values)
    signal settingsViewValuesChanged(var values, bool isInitial)

    // List models for Manage Integrations Dialog.
    property alias requestListModel: requestListModel
    property alias engineListModel: engineListModel

    NxObject
    {
        id: impl

        ListModel
        {
            id: requestListModel

            property var requestByEngineId: ({})

            function add(request)
            {
                if (request.engineId)
                    requestByEngineId[request.engineId] = request

                const engineInfo = request.engineId
                    ? findEngineInfo(NxGlobals.uuid(request.engineId))
                    : engineInfoFromRequest(request)

                append({
                    name: engineInfo.name,
                    engineId: request.engineId ?? "",
                    requestId: request.requestId,
                    type: engineInfo.type
                })
            }

            function set(requests)
            {
                clear()
                requestByEngineId = {}
                if (requests)
                    requests.forEach(request => add(request))
                engineListModel.update()
            }
        }

        ListModel
        {
            id: engineListModel

            signal engineRequestChanged(var engineId, var requestId)

            function add(engine)
            {
                append({
                    name: engine.name,
                    engineId: engine.id.toString(),
                    requestId: "",
                    type: engine.type,
                    visible: true,
                })
            }

            function set(engines)
            {
                engineListModel.clear()
                engines.forEach(engine => add(engine))
                update()
            }

            /** Updates requestId field of all engines. */
            function update()
            {
                for (let i = 0; i < count; i++)
                {
                    let item = get(i)
                    const request = requestListModel.requestByEngineId[item.engineId]
                    const requestId = request && request.requestId || ""
                    if (item.requestId !== requestId)
                    {
                        // Hide the item in the list since it has the request, which is displayed
                        // in requests.
                        item.visible = !requestId
                        item.requestId = requestId
                        engineRequestChanged(NxGlobals.uuid(item.engineId), requestId)
                    }
                }
            }
        }

        onEnginesChanged:
        {
            engineListModel.set(engines)
        }

        onRequestsChanged:
        {
            requestListModel.set(requests)
            const isCurrentRequestNew = !!currentRequestId && !currentEngineId

            // The corresponding engine will be selected by name once it's added if it's not in the
            // engine list.
            if (isCurrentRequestNew && !findRequest(currentRequestId))
                switchToEngineByName(currentRequestEngineName)
        }

        Connections
        {
            target: engineListModel
            function onEngineRequestChanged(engineId, requestId)
            {
                if (engineId === currentEngineId)
                    setCurrentItem("Engine", currentEngineId, currentSectionId, requestId)
            }
        }

        // Common state properties.
        property var engines: []
        property var currentEngineSettingsModel
        property var currentSettingsErrors
        property var currentEngineLicenseSummary
        property var currentRequestId
        property var requests: requestsModel ? requestsModel.requests : null
        property string currentRequestEngineName //< Find the engine by name from a request.

        // Menu related properties.
        property var currentEngineId
        property var currentItemType
        property var currentSectionId
    }

    Component.onCompleted: switchToDefaultState()

    function switchToDefaultState()
    {
        setCurrentItem(
            LocalSettings.iniConfigValue("integrationsManagement") ? "Placeholder" : "Plugins",
            /*engineId*/ null,
            /*sectionId*/ null,
            /*requestId*/ null)

        updateModel(/*model*/ null, /*initialValues*/ null)
    }

    function updateState(engines, licenseSummary, model, values, errors, isInitial)
    {
        if (JSON.stringify(impl.engines) !== JSON.stringify(engines))
            impl.engines = engines

        if (impl.currentEngineLicenseSummary !== licenseSummary)
            impl.currentEngineLicenseSummary = licenseSummary

        if (currentEngineId && findEngineInfo(currentEngineId))
        {
            // currentEngineId is in the engine list.
            if (typeof(model) !== "undefined")
                updateModel(model, values, isInitial)

            impl.currentSettingsErrors = errors
        }
        else if (!currentRequest && !currentEngineId && impl.currentRequestEngineName)
        {
            // Only a name of the engine from a request is available (for example, a new request
            // has been approved).
            switchToEngineByName(impl.currentRequestEngineName)
        }
        else if (currentEngineId)
        {
            // currentEngineId is no longer in the engine list.
            switchToDefaultState()
        }
    }

    function switchToEngineByName(name)
    {
        const engine = findEngineInfoByName(name)
        if (engine)
        {
            setCurrentItem(
                "Engine", engine.id, /*currentSectionId*/ null, /*currentRequestId*/ null)
        }
        else
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

            if (actualModel && !(currentSectionId in sections))
            {
                // Current section id is not found in the new model.
                setCurrentItem(
                    impl.currentItemType,
                    impl.currentEngineId,
                    /*currentSectionId*/ null,
                    impl.currentRequestId)
            }

            settingsViewStateChanged(currentEngineSettingsModel, values)
        }
    }

    /**
     * Sets the current item, requesting engine if needed. Used by a menu or this model when
     * switching to other menu item is needed.
     * @param type Item type:
     *     - Engine (SettingsView)
     *     - Placeholder (feature: integrationsManagement)
     *     - Plugins (Plugins information, feature: enableMetadataApi)
     *     - ApiIntegrations (Integrations information, feature: enableMetadataApi)
     */
    function setCurrentItem(type, engineId, sectionId, requestId)
    {
        if (impl.currentItemType !== type)
            impl.currentItemType = type

        if (impl.currentSectionId !== sectionId)
            impl.currentSectionId = sectionId

        if (impl.currentRequestId !== requestId)
            impl.currentRequestId = requestId

        const resetModel = type !== "Engine" || !!requestId && !engineId
        if (resetModel)
            updateModel(/*model*/ null, /*values*/ null, /*isInitial*/ true)

        // Save the name of the engine to make it possible to find the new engine after its request
        // is approved. Reset only when an engine is explicitly selected, even when switched to the
        // default state.
        if (engineId)
            impl.currentRequestEngineName = ""
        else if (currentRequest)
            impl.currentRequestEngineName = currentRequest.name

        if (impl.currentEngineId != engineId)
        {
            impl.currentEngineId = engineId

            // Keep the underlying model in the actual state even if the engine id is null.
            requestEngine(engineId)
        }
    }

    /**
     * Sets current engine (engineRequested is emitted).
     */
    function setCurrentEngine(engineId)
    {
        setCurrentItem("Engine", engineId, /*sectionId*/ null, /*requestId*/ null)
    }

    function requestEngine(engineId)
    {
        viewModel.engineRequested(engineId)
    }

    function findEngineInfo(engineId)
    {
        return engines.find(engine => engine.id === engineId)
    }

    function findEngineInfoByName(name)
    {
        return engines.find(engine => engine.name === name)
    }

    function findRequest(id)
    {
        return requests ? requests.find(request => request.requestId === id) : null
    }

    function engineInfoFromRequest(request)
    {
        return request
            ? {
                name: request.name ?? "",
                version: request.version ?? "",
                description: request.description ?? "",
                vendor: request.vendor ?? "",
                type: "api"
            }
            : null
    }

    function preprocessedModel(model)
    {
        let modelCopy = JSON.parse(JSON.stringify(model))
        Settings.preprocessModel(modelCopy)
        return modelCopy
    }
}
