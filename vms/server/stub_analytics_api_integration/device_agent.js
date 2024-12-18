// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

"use strict"

const constants = require("./constants");
const { mergeMaps } = require("./utils.js");
const { randomUUID } = require("node:crypto");

const makeObjectMetadataPackets = (mouseMovableObjectMetadata, objectActionMetadata, timestampUs) => {
    if (!objectActionMetadata.trackId ||
        objectActionMetadata.boundingBox.x + objectActionMetadata.boundingBox.width > 1.0)
    {
      objectActionMetadata.trackId = randomUUID();
      objectActionMetadata.boundingBox.x = 0;
      objectActionMetadata.boundingBox.y = 0.33;
      objectActionMetadata.boundingBox.width = 0.2;
      objectActionMetadata.boundingBox.height = 0.33;
    }
    else
    {
      objectActionMetadata.boundingBox.x += 1.0 / 70;
    }

    return [
        {
            timestampUs: timestampUs,
            durationUs: 30000,
            objects: [mouseMovableObjectMetadata]
        },
        {
            timestampUs: timestampUs,
            durationUs: 30000,
            objects: [objectActionMetadata]
        }
    ];
};

const makeEventMetadataPacket = (trackId, timestampUs) => {
    return {
        timestampUs: timestampUs,
        durationUs: 0,
        events:
        [
            {
                typeId: "analytics.api.stub.lineCrossing",
                caption: "Analytics API Stub: Line crossing (caption)",
                description: "Analytics API Stub: Line crossing (description)",
                isActive: true,
                trackId: trackId,
                key: ""
            }
        ]
    };
};

const makeBestShotMetadataPacket = (trackId, timestampUs) => {
    return {
        timestampUs: timestampUs,
        trackId: trackId,
        imageUrl: "https://picsum.photos/200/300"
    };
};

const makeObjectTitleMetadataPacket = (trackId, timestampUs) => {
    return {
        timestampUs: timestampUs,
        trackId: trackId,
        imageUrl: "https://picsum.photos/200/300",
        text: "Analytics API Stub: Object Track title"
    };
};

class DeviceAgent {
    manifests = {};
    deviceAgentSettingsModel = {};
    target = {
        id: "", // Engine Id
        deviceId: ""
    };
    lastDataTimestampUs = -1;
    mouseMovableObjectMetadata = null;
    appContext = null;
    frameCounter = 0;
    lastTrackId = null;
    objectActionMetadata = {
        typeId: "analytics.api.stub.objectTypeWithActions",
        trackId: null,
        boundingBox: {
            x: 0,
            y: 0,
            width: 0,
            height: 0
        }
    };

    constructor(engineId, deviceInfo, mouseMovableObjectMetadata, appContext, manifests)
    {
        this.manifests = manifests;
        this.deviceAgentSettingsModel = manifests.engineManifest.deviceAgentSettingsModel;
        this.target.id = engineId;
        this.target.deviceId = deviceInfo.id;
        this.mouseMovableObjectMetadata = mouseMovableObjectMetadata;
        this.appContext = appContext;
    }

    manifest()
    {
        return this.manifests.deviceAgentManifest;
    }

    pushData(data)
    {
        console.log("DeviceAgent::pushData");

        this.frameCounter = this.frameCounter + 1;

        this.lastDataTimestampUs = data.timestampUs ?? this.lastDataTimestampUs;

        // Send object metadata packets on every frame.
        let objectMetadataPackets = makeObjectMetadataPackets(this.mouseMovableObjectMetadata,
            this.objectActionMetadata,
            this.lastDataTimestampUs);

        let method = constants.PUSH_DEVICE_AGENT_OBJECT_METADATA_METHOD;

        for (let packet of objectMetadataPackets)
        {
            let parameters = mergeMaps(this.target, packet);
            this.appContext.rpcClient.notify(method, parameters);
        }

        // Send event metadata, best shot metadata and object title metadata only once per track,
        // based on the trackId of objectActionMetadata.
        if (this.lastTrackId === this.objectActionMetadata.trackId)
            return;

        this.lastTrackId = this.objectActionMetadata.trackId;

        {
            let method = constants.PUSH_DEVICE_AGENT_OBJECT_TITLE_METADATA_METHOD;

            let packet = makeObjectTitleMetadataPacket(this.objectActionMetadata.trackId,
                this.lastDataTimestampUs);
            let parameters = mergeMaps(this.target, packet);
            this.appContext.rpcClient.notify(method, parameters);
        }
    }

    pushManifest()
    {
        console.log("DeviceAgent::pushManifest");

        let method = constants.PUSH_DEVICE_AGENT_MANIFEST_METHOD;

        let notificationObject = {
            deviceAgentManifest: this.manifest()
        };

        let parameters = mergeMaps(this.target, notificationObject);

        console.log(method, JSON.stringify(parameters, null, 4));

        this.appContext.rpcClient.notify(
            method,
            parameters);
    }

    setSettings(settingsValues)
    {
        console.log("DeviceAgent::setSettings");

        const settingsResponse = {
            settingsModel: {},
            settingsValues: settingsValues,
            // settingsErrors: {}
        };

        const newModel = JSON.parse(JSON.stringify(this.deviceAgentSettingsModel));

        const additionalCheckboxName = "Additional checkbox";

        if (settingsValues["Test Device Agent's setSettings"] === "true")
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

        this.deviceAgentSettingsModel = newModel;
        settingsResponse.settingsModel = newModel;

        let response = {
            type: "ok",
            data: settingsResponse
        };

        return response;
    }


    getSettingsOnActiveSettingChange(activeSettingChangedAction)
    {
        console.log("DeviceAgent::getSettingsOnActiveSettingChange");

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

        const newModel = JSON.parse(JSON.stringify(this.deviceAgentSettingsModel));

        const additionalActiveCheckboxName = "Additional Active checkbox";

        if (settingsResponse.settingsValues["Test Device Agent's Active Settings"] === "true")
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

        this.deviceAgentSettingsModel = newModel;
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
        console.log("DeviceAgent::pushPluginDiagnosticEvent");

        let method = constants.PUSH_DEVICE_AGENT_PLUGIN_DIAGNOSTIC_EVENT_METHOD;

        let notificationObject = {
            level: level,
            caption: caption,
            description: description
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
    DeviceAgent: DeviceAgent
};
