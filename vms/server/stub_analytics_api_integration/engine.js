// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

"use strict"

const constants = require("./constants");
const { mergeMaps } = require("./utils.js");

const DeviceAgent = require("./device_agent.js").DeviceAgent;

class Engine {
    manifests = {}
    engineId = "";
    mouseMovableObjectMetadata = null;
    appContext = null;
    engineSettingsModel = {};

    constructor(engineId, mouseMovableObjectMetadata, appContext, manifests) {
        this.manifests = manifests;
        this.engineSettingsModel = manifests.integrationManifest.engineSettingsModel;
        this.engineId = engineId;
        this.mouseMovableObjectMetadata = mouseMovableObjectMetadata;
        this.appContext = appContext;
        this.target = {
            id: engineId // Engine Id
        };
    }

    manifest()
    {
        return this.manifests.engineManifest;
    }

    obtainDeviceAgent(deviceInfo)
    {
        return new DeviceAgent(this.engineId,
            deviceInfo,
            this.mouseMovableObjectMetadata,
            this.appContext,
            this.manifests);
    }

    compatibilityInfo(deviceInfo)
    {
        return { isCompatible: true };
    }

    setSettings(settingsValues)
    {
        const settingsResponse = {
            settingsModel: {},
            settingsValues: settingsValues,
            // settingsErrors: {}
        };

        const newModel = JSON.parse(JSON.stringify(this.engineSettingsModel));

        const additionalCheckboxName = "Additional checkbox";

        if (settingsValues["Test Engine's setSettings"] === "true")
        {
            const additionalCheckbox = {
                "type": "CheckBox",
                "name": additionalCheckboxName,
                "caption": additionalCheckboxName,
                "defaultValue": false
            };

            if (!newModel.items.some(obj => obj.name === additionalCheckboxName))
            {
                newModel.items.push(additionalCheckbox);
            }
        }
        else
        {
            newModel.items = newModel.items.filter(obj => obj.name !== additionalCheckboxName);
        }

        this.engineSettingsModel = newModel;
        settingsResponse.settingsModel = newModel;

        // Example of error response:

        let errorResponse = {
            type: "error",
            error: {
                errorCode: "networkError",
                errorMessage: "Network Error"
            }
        };

        let response = {
            type: "ok",
            data: settingsResponse
        };

        return response;
    }

    getSettingsOnActiveSettingChange(activeSettingChangedAction)
    {
        /*
        struct ActiveSettingChangedAction
        {
            std::string activeSettingName;
            QJsonObject settingsModel;
            std::map<std::string, std::string> settingsValues;
            std::map<std::string, std::string> params;
        };
        */

        const actionResponse = {
            actionUrl: "",
            messageToUser: "",
            useProxy: false,
            useDeviceCredentials: false
        };

        const settingsResponse = {
            settingsModel: {},
            settingsValues: activeSettingChangedAction.settingsValues,
            // settingsErrors: {}
        };

        const newModel = JSON.parse(JSON.stringify(this.engineSettingsModel));

        const additionalActiveCheckboxName = "Additional Active checkbox";

        if (settingsResponse.settingsValues["Test Engine's Active Settings"] === "true")
        {
            const additionalActiveCheckbox = {
                "type": "CheckBox",
                "name": additionalActiveCheckboxName,
                "caption": additionalActiveCheckboxName,
                "defaultValue": false
            };

            if (!newModel.items.some(obj => obj.name === additionalActiveCheckboxName))
            {
                newModel.items.push(additionalActiveCheckbox);
            }
        }
        else
        {
            newModel.items = newModel.items.filter(obj => obj.name !== additionalActiveCheckboxName);
        }

        this.engineSettingsModel = newModel;
        settingsResponse.settingsModel = newModel;

        const activeSettingChangedResponse = {
            actionResponse: actionResponse,
            settingsResponse: settingsResponse
        };

        let response = {
            type: "ok",
            data: activeSettingChangedResponse
        };

        return response;
    }

    // `level` is one of "info", "warning", "error".
    pushPluginDiagnosticEvent(level, caption, description)
    {
        console.log("Engine::pushPluginDiagnosticEvent");

        let method = constants.PUSH_ENGINE_PLUGIN_DIAGNOSTIC_EVENT_METHOD;

        let notificationObject = {
            level: level,
            caption: caption,
            description: description
        };

        let parameters = mergeMaps(this.target, notificationObject);

        console.log(method, JSON.stringify(parameters, null, 4));

        this.appContext.rpcClient.notify(method, parameters);
    }

    executeAction(action)
    {
        /*
            struct ObjectTrackInfo
            {
                std::string titleText;
                QByteArray titleImageData;
                int titleImageDataSize;
                std::string titleImageDataFormat;
            };

            struct Action
            {
                std::string actionId;
                Uuid objectTrackId;
                Uuid deviceId;
                ObjectTrackInfo objectTrackInfo;
                int64_t timestampUs;
                std::map<std::string, std::string> params
            };
        */

        console.log("Got action: ", action);

        const actionResult = {
            actionUrl: "",
            messageToUser: "Hello from Object Action",
            useProxy: false,
            useDeviceCredentials: false
        };

        let response = {
            type: "ok",
            data: actionResult
        };

        return response;
    }

    pushManifest()
    {
        let method = constants.PUSH_ENGINE_MANIFEST_METHOD;

        let notificationObject = {
            engineManifest: this.manifest()
        };

        let parameters = mergeMaps(this.target, notificationObject);

        console.log(method, JSON.stringify(parameters, null, 4));

        this.appContext.rpcClient.notify(method, parameters);
    }

    getIntegrationSideSettings()
    {
        return {
            type: "ok",
            data: {}
        };
    }
}

module.exports = {
    Engine: Engine
};
