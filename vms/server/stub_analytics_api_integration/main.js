// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

"use strict"

const { app, BrowserWindow, ipcMain, session } = require("electron");
const path = require("node:path");
const fs = require('fs');
const https = require("node:https");
const { randomUUID } = require("node:crypto");

const JsonRpcClient = require("./json_rpc_client").JsonRpcClient;
const Engine = require("./engine.js").Engine;
const Integration = require("./integration.js").Integration;
const constants = require("./constants.js");
const { createLogger } = require("./utils.js");
const { readFileSync } = require("node:fs");
const axios = require('axios');

const axiosInstance = axios.create({
  httpsAgent: new https.Agent({
    rejectUnauthorized: false
  })
})

const kUpdateTimestampStatsIntervalMs = 5000;
const kUpdateTimestampGapWarningMs = 2000;

const logger = createLogger("MAIN");
const webContentsConsoleLevelToText = (level) => {
    switch (level)
    {
        case 0: return "verbose";
        case 1: return "info";
        case 2: return "warning";
        case 3: return "error";
        default: return `unknown(${level})`;
    }
};

const summarizeIceServersForLog = (iceServers) => {
    if (!Array.isArray(iceServers))
    {
        return [];
    }

    return iceServers.map((entry) => {
        const urls = Array.isArray(entry && entry.urls)
            ? entry.urls
            : (entry && typeof entry.urls === "string" ? [entry.urls] : []);
        return {
            urls: urls,
            hasUsername: !!(entry && entry.username),
            hasCredential: !!(entry && entry.credential)
        };
    });
};

const formatLogValue = (value) => {
    if (value === null)
        return "null";

    if (typeof value === "undefined")
        return "undefined";

    if (value instanceof Error)
        return `${value.name}: ${value.message}`;

    if (Array.isArray(value))
        return value.map((entry) => formatLogValue(entry)).join("|");

    if (typeof value === "object")
    {
        const entries = Object.entries(value);
        if (entries.length === 0)
            return "{}";

        return entries
            .map(([key, entryValue]) => `${key}=${formatLogValue(entryValue)}`)
            .join(", ");
    }

    return String(value);
};

const pushDataContextToLogText = (pushDataContext) => {
    if (!pushDataContext)
        return "exists=false";

    const timestampMs = pushDataContext.pushDataInfo ? pushDataContext.pushDataInfo.timestampMs : null;
    const targetEngineId = pushDataContext.deviceAgent && pushDataContext.deviceAgent.target
        ? pushDataContext.deviceAgent.target.id
        : null;
    const targetDeviceId = pushDataContext.deviceAgent && pushDataContext.deviceAgent.target
        ? pushDataContext.deviceAgent.target.deviceId
        : null;

    return `exists=true, connected=${pushDataContext.connected}, frameCount=${pushDataContext.frameCount}, `
        + `pdeFromEngine=${pushDataContext.pdeFromEngine}, timestampMs=${timestampMs}, `
        + `targetEngineId=${targetEngineId}, targetDeviceId=${targetDeviceId}`;
};

class IntegrationContext {
    integration = null;
    engineContextById = {};
    deviceAgentPushDataContext = null;

    constructor(manifests, mouseMovableObjectMetadata, rpcClient) {
        this.integration = new Integration(manifests, mouseMovableObjectMetadata, rpcClient);
        this.integrationUserName = null;
        this.integrationPassword = null;
        this.integrationUserId = null;
    }
};

class EngineContext {
    engine = null;
    deviceAgentsByDeviceId = {};

    constructor(engineId, mouseMovableObjectMetadata, appContext, manifests) {
        this.engine = new Engine(engineId, mouseMovableObjectMetadata, appContext, manifests);
    }
}

class AppContext  {
    manifests = {};
    config = null;
    integrationContext = null;
    rpcClient = null;
    window = null;
    mouseMovableObjectMetadata = {
        typeId: "analytics.api.stub.object.type",
        trackId: randomUUID(),
        boundingBox: {
            x: 0,
            y: 0,
            width: 0,
            height: 0
        },
        attributes: [
            {
                name: "attribute 1",
                value: "attribute value 1",
            },
            {
                name: "attribute 2",
                value: "attribute value 2",
            },
            {
                name: "attribute 3",
                value: "attribute value 3",
            }
        ]
    };

    viewModel = {
        connectionState: "disconnected"
    };

    constructor(config, manifests, rpcClient) {
        logger.log("Initializing AppContext.");

        this.manifests = manifests;
        this.config = config;
        this.integrationContext = new IntegrationContext(
            manifests,
            this.mouseMovableObjectMetadata,
            rpcClient);
        this.rpcClient = rpcClient;
        this.adminSessionToken = null;
        this.integrationUserSessionToken = null;
        this.updateTimestampDiagnostics = {
            windowStartMs: Date.now(),
            eventCountInWindow: 0,
            lastTimestampMs: null,
            lastUpdateWallclockMs: null
        };
    }
};

let appContext = null;

app.on('ready', () => {
    logger.log("Electron app ready.");

    // Ignore SSL certificate errors
    app.on('certificate-error', (event, webContents, url, error, certificate, callback) => {
        logger.log(
            `certificate-error intercepted. Allowing connection. url=${url}, error=${formatLogValue(error)}`
        );
        event.preventDefault();
        callback(true); // Bypass the error
    });
});

const initializeWebRtc = (appContext, deviceId) => {
    logger.log(`initializeWebRtc requested. deviceId=${deviceId} hasWindow=${!!(appContext && appContext.window)}`);

    if (appContext.adminSessionToken === null)
    {
        logger.warn("initializeWebRtc skipped: Admin session token is null.");
        return;
    }

    const config = JSON.parse(JSON.stringify(appContext.config));
    config.adminSessionToken = appContext.adminSessionToken;
    config.deviceId = deviceId;

    const iceServersSummary = summarizeIceServersForLog(config.iceServers)
        .map((server, index) =>
            `#${index} urls=${formatLogValue(server.urls)} hasUsername=${server.hasUsername} hasCredential=${server.hasCredential}`)
        .join("; ");

    logger.log(
        `Sending initialize-webrtc to renderer. deviceId=${deviceId}, host=${config.vmsHost}, `
        + `port=${config.vmsPort}, iceTransportPolicy=${config.iceTransportPolicy || "all"}, `
        + `iceServers=${iceServersSummary}`
    );
    appContext.window.webContents.send("initialize-webrtc", config);
}

const createWindow = () => {
    logger.log("Creating browser window.");

    const window = new BrowserWindow({
        width: 800,
        height: 600,
        webPreferences: {
            preload: path.resolve(__dirname, "preload.js"),
            nodeIntegration: true,
            contextIsolation: true,
            enableRemoteModule: true,
        }
    });

    window.once('ready-to-show', () => {
        logger.log("Window ready-to-show.");
    });

    window.webContents.on("console-message", (_event, level, message, line, sourceId) => {
        logger.log(
            `Renderer console message. level=${webContentsConsoleLevelToText(level)}, `
            + `source=${sourceId}, line=${line}, message=${message}`
        );
    });

    logger.log("Loading index.html.");
    window.loadFile("index.html");
    logger.log("Opening dev tools.");
    window.webContents.openDevTools();

    logger.log("Browser window created.");
    return window;
}

function findOrCreateEngineContextById(engineId)
{
    const engineContexts = appContext.integrationContext.engineContextById;

    if (!engineContexts.hasOwnProperty(engineId))
    {
        logger.log(`Creating EngineContext. engineId=${engineId}`);
        engineContexts[engineId] =
            new EngineContext(engineId, appContext.mouseMovableObjectMetadata, appContext,
                appContext.manifests);
    }
    else
    {
        logger.log(`Reusing EngineContext. engineId=${engineId}`);
    }

    return engineContexts[engineId];
}

function findOrCreateEngineById(engineId)
{
    const engineContext = findOrCreateEngineContextById(engineId);

    return engineContext.engine;
}

function findDeviceAgentByIdOrThrow(engineId, deviceId)
{
    const engineContext = findOrCreateEngineContextById(engineId);

    const deviceAgents = engineContext.deviceAgentsByDeviceId;

    if (!deviceAgents.hasOwnProperty(deviceId))
    {
        logger.error(
            `Device Agent not found in cache. engineId=${engineId}, deviceId=${deviceId}, `
            + `knownDeviceIds=${Object.keys(deviceAgents).join("|")}`
        );
        throw new Error('Device Agent not found');
    }

    logger.log(`Resolved Device Agent from cache. engineId=${engineId}, deviceId=${deviceId}`);
    return deviceAgents[deviceId];
}

const initialize = async () => {
    let config = JSON.parse(readFileSync("config.json"));
    logger.log("Initializing application.");

    let manifests = {
        integrationManifest: JSON.parse(readFileSync("integration_manifest.json", "utf8")),
        engineManifest: JSON.parse(readFileSync("engine_manifest.json", "utf8")),
        deviceAgentManifest: JSON.parse(readFileSync("device_agent_manifest.json", "utf8"))
    };

    logger.log("Read config. " + formatLogValue(config));
    logger.log(
        `Read manifests. hasIntegrationManifest=${!!manifests.integrationManifest}, `
        + `hasEngineManifest=${!!manifests.engineManifest}, `
        + `hasDeviceAgentManifest=${!!manifests.deviceAgentManifest}`
    );

    appContext = new AppContext(
        config,
        manifests,
        await initializeRpcClient()
        );

    const hasIntegrationCredentials = fs.existsSync("integration_credentials.json");
    logger.log("integration_credentials.json exists: " + formatLogValue(hasIntegrationCredentials));
    if (hasIntegrationCredentials)
    {
        logger.log("Reading integration_credentials.json.");

        let data = JSON.parse(readFileSync("integration_credentials.json"));

        logger.log(
            `Read integration credentials. integrationUserName=${data.integrationUserName}, `
            + `integrationUserId=${data.integrationUserId}`
        );

        appContext.integrationContext.integrationUserName = data.integrationUserName;
        appContext.integrationContext.integrationPassword = data.integrationPassword;
        appContext.integrationContext.integrationUserId = data.integrationUserId;
    }
    else
    {
        logger.warn("integration_credentials.json doesn't exist, make integration request and approve it first.");
    }

    const hasIntegrationState = fs.existsSync("integration_state.json");
    logger.log("integration_state.json exists: " + formatLogValue(hasIntegrationState));
    if (hasIntegrationState)
    {
        logger.log("Reading integration_state.json.");

        let integration_state = JSON.parse(readFileSync("integration_state.json"));

        logger.log(`Read integration state. deviceCount=${Object.keys(integration_state).length}`);

        for (const [deviceId, data] of Object.entries(integration_state))
        {
            let engineId = data.engineId;
            let deviceInfo = data.deviceInfo;

            const engineContext = findOrCreateEngineContextById(engineId);
            const deviceAgents = engineContext.deviceAgentsByDeviceId;

            if (!deviceAgents.hasOwnProperty(deviceId))
            {
                const engine = engineContext.engine;
                const deviceAgent = engine.obtainDeviceAgent(deviceInfo);
                deviceAgents[deviceId] = deviceAgent;
                logger.log(
                    `Restored device agent from integration_state. engineId=${engineId}, deviceId=${deviceId}`
                );
            }
        }
    }

    logger.log("Logging in as admin and requesting admin token.");

    appContext.adminSessionToken = await getSessionToken(
        appContext.config.vmsUser,
        appContext.config.vmsAdminPassword);
}

const getSessionToken = async (user, password) => {
    logger.log(`Getting session token. user=${user}`);

    const path = "rest/v1/login/sessions";
    const route = `https://${appContext.config.vmsHost}:${appContext.config.vmsPort}/${path}`;

    const data = {
        username: user,
        password: password
    }

    const config = {
        headers: {
            'Content-Type': 'application/json'
        },
        httpsAgent: new https.Agent({
            rejectUnauthorized: false, //< Disables certificate validation.
        })
    };

    logger.log("getSessionToken request route: " + formatLogValue(route));
    logger.log("getSessionToken config: " + formatLogValue(config));
    logger.log(
        `getSessionToken payload: username=${data.username}, `
        + `passwordLength=${data.password ? data.password.length : 0}`
    );

    try
    {
        const response = await axiosInstance.post(route, data, config);
        logger.log(`Session token response received. user=${user}, hasToken=${!!response.data.token}`);
        return response.data.token;
    }
    catch (error)
    {
        logger.error(`getSessionToken failed. user=${user}, error=${formatLogValue(error)}`);
        return null;
    }
}

const authorizeIntegrationAndConnect = async () => {
    logger.log("Authorizing integration and connecting.");
    const url =
        "wss://"
        + appContext.config.vmsHost
        + ":"
        + appContext.config.vmsPort
        + "/jsonrpc";

    appContext.integrationUserSessionToken = await getSessionToken(
        appContext.integrationContext.integrationUserName,
        appContext.integrationContext.integrationPassword);

    if (!appContext.integrationUserSessionToken)
    {
        logger.error("Failed to get integration user session token.");
        return false;
    }

    logger.log(
        `Connecting RPC client websocket. url=${url}, `
        + `integrationUserName=${appContext.integrationContext.integrationUserName}`
    );
    appContext.rpcClient.connect(url.toString(), {
        rejectUnauthorized: false,
        headers: {
            "Authorization": "Bearer " + appContext.integrationUserSessionToken
        }
    });

    logger.log("RPC websocket connect() called.");
    return true;
}

const initializeRpcClient = async () => {
    logger.log("Initializing RPC client.");
    let rpcClient = new JsonRpcClient();

    rpcClient.on(constants.CREATE_DEVICE_AGENT_METHOD, async (data) => {
        logger.log(
            `RPC request received: CREATE_DEVICE_AGENT_METHOD. method=${constants.CREATE_DEVICE_AGENT_METHOD}`
        );
        logger.log("Device Agent requested payload= " + formatLogValue(data));

        const engineId = data.params.target.engineId;
        const deviceInfo = data.params.parameters;
        const deviceId = deviceInfo.id;

        logger.log(`CREATE_DEVICE_AGENT_METHOD target parsed. engineId=${engineId}, deviceId=${deviceId}`);

        const engineContext = findOrCreateEngineContextById(engineId);
        const deviceAgents = engineContext.deviceAgentsByDeviceId;

        function initializePushData(engine, deviceAgent)
        {
            if (!appContext.integrationContext.deviceAgentPushDataContext)
            {
                appContext.integrationContext.deviceAgentPushDataContext =
                {
                    deviceAgent: deviceAgent,
                    engine: engine,
                    frameCount: 0,
                    pdeFromEngine: true, //< True - from Engine, false - from DeviceAgent.
                    pushDataInfo: { timestampMs: 1 },
                    connected: true
                };
                logger.log(
                    `Initialized deviceAgentPushDataContext. ${pushDataContextToLogText(
                        appContext.integrationContext.deviceAgentPushDataContext
                    )}`);
            }
            else
            {
                logger.log(
                    `deviceAgentPushDataContext already exists. ${pushDataContextToLogText(
                        appContext.integrationContext.deviceAgentPushDataContext
                    )}`);
            }
        }

        if (!deviceAgents.hasOwnProperty(deviceId))
        {
            const engine = engineContext.engine;
            const deviceAgent = engine.obtainDeviceAgent(deviceInfo);
            deviceAgents[deviceId] = deviceAgent;
            logger.log(`Created new device agent for device. engineId=${engineId}, deviceId=${deviceId}`);
        }
        else
        {
            logger.log(`Device agent already exists for device. engineId=${engineId}, deviceId=${deviceId}`);
        }

        initializePushData(engineContext.engine, deviceAgents[deviceId]);
        initializeWebRtc(appContext, deviceId);

        if (!fs.existsSync("integration_state.json"))
            fs.writeFileSync("integration_state.json", JSON.stringify({}));

        let integration_state = JSON.parse(readFileSync("integration_state.json"));

        integration_state[deviceId] = {
            engineId: engineId,
            deviceInfo: deviceInfo
        };

        fs.writeFileSync("integration_state.json", JSON.stringify(integration_state));
        logger.log(
            `Persisted integration_state.json for device. engineId=${engineId}, `
            + `deviceId=${deviceId}, totalDeviceCount=${Object.keys(integration_state).length}`
        );

        logger.log(`CREATE_DEVICE_AGENT_METHOD completed successfully. engineId=${engineId}, deviceId=${deviceId}`);
        return {
            type: "ok",
            data: {
                deviceAgentManifest: deviceAgents[deviceId].manifest()
            }
        };
    });

    rpcClient.on(constants.DELETE_DEVICE_AGENT_METHOD, async (data) => {
        logger.log(
            `RPC notification received: DELETE_DEVICE_AGENT_METHOD. method=${constants.DELETE_DEVICE_AGENT_METHOD}`
        );
        logger.log("Delete Device Agent payload= " + formatLogValue(data));

        const engineId = data.params.target.engineId;
        const deviceId = data.params.target.deviceId;

        let engineContext = findOrCreateEngineContextById(engineId);
        let deviceAgents = engineContext.deviceAgentsByDeviceId;

        if (deviceAgents.hasOwnProperty(deviceId))
        {
            if (fs.existsSync("integration_state.json"))
            {
                let integration_state = JSON.parse(readFileSync("integration_state.json"));
                delete integration_state[deviceId];
                fs.writeFileSync("integration_state.json", JSON.stringify(integration_state));
                logger.log(
                    `Removed device from integration_state.json. engineId=${engineId}, `
                    + `deviceId=${deviceId}, totalDeviceCount=${Object.keys(integration_state).length}`
                );
            }

            if (appContext.integrationContext.deviceAgentPushDataContext)
            {
                logger.log("Stopping push data loop.");
                appContext.integrationContext.deviceAgentPushDataContext = null;
            }
            else
            {
                logger.warn("Push data loop is already stopped.");
            }
        }
        else
        {
            logger.warn(
                `DELETE_DEVICE_AGENT_METHOD ignored: device agent does not exist. `
                + `engineId=${engineId}, deviceId=${deviceId}`
            );
        }
    });

    rpcClient.on(constants.SET_ENGINE_SETTINGS_METHOD, async (data) => {
        logger.log("Engine setSettings request= " + formatLogValue(data));

        const engineId = data.params.target.engineId;
        const settingsRequest = data.params.parameters;
        const settingsValues = settingsRequest.settingsValues;

        const engine = findOrCreateEngineById(engineId);

        const settingsResponse = engine.setSettings(settingsValues);

        logger.log("Response= " + formatLogValue(settingsResponse));

        return settingsResponse;
    });

    rpcClient.on(constants.SET_DEVICE_AGENT_SETTINGS_METHOD, async (data) => {
        logger.log("Device Agent setSettings request= " + formatLogValue(data));

        const engineId = data.params.target.engineId;
        const deviceId = data.params.target.deviceId;
        const settingsRequest = data.params.parameters;
        const settingsValues = settingsRequest.settingsValues;

        const deviceAgent = findDeviceAgentByIdOrThrow(engineId, deviceId);

        let settingsResponse = deviceAgent.setSettings(settingsValues);

        logger.log("Response= " + formatLogValue(settingsResponse));

        return settingsResponse;
    });

    rpcClient.on(constants.GET_ENGINE_SETTINGS_ON_ACTIVE_SETTING_CHANGE, async (data) => {
        logger.log("Engine getSettingsOnActiveSettingChange request= " + formatLogValue(data));

        const engineId = data.params.target.engineId;
        const activeSettingChangedAction = data.params.parameters;

        const engine = findOrCreateEngineById(engineId);

        const activeSettingChangedResponse =
            engine.getSettingsOnActiveSettingChange(activeSettingChangedAction);

        logger.log("Response= " + formatLogValue(activeSettingChangedResponse));

        return activeSettingChangedResponse;
    });

    rpcClient.on(constants.GET_DEVICE_AGENT_SETTINGS_ON_ACTIVE_SETTING_CHANGE, async (data) => {
        logger.log("Device Agent getSettingsOnActiveSettingChange request= " + formatLogValue(data));

        const engineId = data.params.target.engineId;
        const deviceId = data.params.target.deviceId;
        const activeSettingChangedAction = data.params.parameters;

        const deviceAgent = findDeviceAgentByIdOrThrow(engineId, deviceId);

        const activeSettingChangedResponse =
            deviceAgent.getSettingsOnActiveSettingChange(activeSettingChangedAction);

        logger.log("Response= " + formatLogValue(activeSettingChangedResponse));

        return activeSettingChangedResponse;
    });

    rpcClient.on(constants.EXECUTE_ENGINE_ACTION, async (data) => {
        logger.log("Engine executeEngineAction request= " + formatLogValue(data));

        const engineId = data.params.target.engineId;
        const action = data.params.parameters;

        const engine = findOrCreateEngineById(engineId);

        const actionResult = engine.executeAction(action);

        logger.log("Result= " + formatLogValue(actionResult));

        return actionResult;
    });

    rpcClient.on(constants.GET_ENGINE_COMPATIBILITY_INFO_METHOD, async (data) => {
        logger.log("Engine compatibility info request= " + formatLogValue(data));

        const engineId = data.params.target.engineId;
        const deviceInfo = data.params.parameters;

        const engine = findOrCreateEngineById(engineId);

        const result = engine.compatibilityInfo(deviceInfo);

        logger.log("Result= " + formatLogValue(result));

        return result;
    });

    rpcClient.on(constants.GET_ENGINE_SIDE_SETTINGS_METHOD, async (data) => {
        logger.log("Engine side settings request= " + formatLogValue(data));

        const engineId = data.params.target.engineId;

        const engine = findOrCreateEngineById(engineId);

        const result = engine.getIntegrationSideSettings();

        logger.log("Result= " + formatLogValue(result));

        return result;
    });

    rpcClient.on(constants.GET_DEVICE_AGENT_SIDE_SETTINGS_METHOD, async (data) => {
        logger.log("Device Agent side settings request= " + formatLogValue(data));

        const engineId = data.params.target.engineId;
        const deviceId = data.params.target.deviceId;

        const deviceAgent = findDeviceAgentByIdOrThrow(engineId, deviceId);

        const result = deviceAgent.getIntegrationSideSettings();

        logger.log("Result= " + formatLogValue(result));

        return result;
    });

    rpcClient.onClose(async () => {
        // This handler will be called only when the server closes the websocket connection.
        logger.log(`RPC onClose handler invoked. connectionState=${appContext.viewModel.connectionState}`);

        // In case the session is expired, try to refresh the session and reconnect.
        if (await authorizeIntegrationAndConnect())
        {
            logger.log("RPC reconnect attempt from onClose succeeded.");
            return;
        }

        appContext.viewModel.connectionState = "disconnected";
        logger.warn("RPC reconnect attempt from onClose failed. Sending disconnected state.");
        appContext.window.webContents.send("state-changed", appContext.viewModel);
    });

    rpcClient.onOpen(() => {
        logger.log("RPC onOpen handler invoked.");

        if (!appContext.integrationContext.integrationUserId)
        {
            logger.warn("RPC onOpen: integrationUserId missing. Make integration request and approve it first.");
            return;
        }

        let userId =  {
            id: appContext.integrationContext.integrationUserId
        };

        rpcClient.call(constants.GET_USER_METHOD, userId)
        .then((result) => {
            logger.log("GET_USER response= " + formatLogValue(result));

            if (result.result.parameters.integrationRequestData.isApproved)
            {
                logger.log("Integration request is approved, subscribing.");

                rpcClient.call(constants.SUBSCRIBE)
                .then((result) => {
                    logger.log("SUBSCRIBE call succeeded. " + formatLogValue(result));
                    appContext.viewModel.connectionState = "connected";
                    logger.log("Sending state-changed event: connected.");
                    appContext.window.webContents.send("state-changed", appContext.viewModel);
                }).catch((error) => {
                    logger.error(`SUBSCRIBE call failed. error=${formatLogValue(error)}`);
                });
            }
            else
            {
                logger.warn("Integration request is not approved. Approve it first.");
            }
        }).catch((error) => {
            logger.error(`GET_USER call failed. error=${formatLogValue(error)}`);
        });

    });

    return rpcClient;
}

const initializeNotifications = () => {
    logger.log("Registering IPC notifications.");

    ipcMain.on("object-move", (event, object) => {
        logger.log("IPC event: object-move. " + formatLogValue(object));
        const boundingBox = appContext.mouseMovableObjectMetadata.boundingBox;

        boundingBox.x = object.x;
        boundingBox.y = object.y;
        boundingBox.width = object.width;
        boundingBox.height = object.height;
        logger.log("Updated mouseMovableObjectMetadata boundingBox. " + formatLogValue(boundingBox));
    });

    ipcMain.on("connect-disconnect", async () => {
        logger.log(`IPC event: connect-disconnect. currentState=${appContext.viewModel.connectionState}`);

        if (appContext.viewModel.connectionState == "connected")
        {
            logger.log("Disconnecting integration RPC client.");
            if (appContext.integrationContext.deviceAgentPushDataContext)
            {
                appContext.integrationContext.deviceAgentPushDataContext.connected = false;
                logger.log(
                    `Marked push data context as disconnected. ${pushDataContextToLogText(
                        appContext.integrationContext.deviceAgentPushDataContext
                    )}`);
            }
            // Disconnect without executing the onClose handler.
            // When the onClose handler is executed, it will try to reconnect, but we don't need
            // to reconnect when the user explicitly disconnects by pressing a button, we only
            // need to reconnect when the websocket connection is closed by the server.
            appContext.rpcClient.disconnect();
            appContext.viewModel.connectionState = "disconnected";
            logger.log("Sending state-changed event: disconnected.");
            appContext.window.webContents.send("state-changed", appContext.viewModel);
        }
        else
        {
            logger.log("Connecting integration RPC client.");

            if (!appContext.adminSessionToken)
            {
                logger.warn("connect-disconnect aborted: not logged in as admin.");
                return;
            }

            if (!appContext.integrationContext.integrationUserName)
            {
                logger.warn("connect-disconnect aborted: integration user credentials missing.");
                return;
            }

            appContext.rpcClient = await initializeRpcClient();

            const connected = await authorizeIntegrationAndConnect();
            logger.log(`authorizeIntegrationAndConnect() finished. connected=${connected}`);

            if (appContext.integrationContext.deviceAgentPushDataContext)
            {
                appContext.integrationContext.deviceAgentPushDataContext.connected = true;
                logger.log(
                    `Marked push data context as connected. ${pushDataContextToLogText(
                        appContext.integrationContext.deviceAgentPushDataContext
                    )}`);
            }
        }
    });

    ipcMain.on("make-integration-request", async () => {
        logger.log("IPC event: make-integration-request.");

        if (!appContext.adminSessionToken)
        {
            logger.warn("make-integration-request aborted: not logged in as admin.");
            return;
        }

        const path = "rest/v4/analytics/integrations/*/requests";
        const route = `https://${appContext.config.vmsHost}:${appContext.config.vmsPort}/${path}`;

        const data = {
            integrationManifest: appContext.integrationContext.integration.manifest(),
            engineManifest: appContext.manifests.engineManifest,
            pinCode: "1234"
        };

        const config = {
            headers: {
                'Content-Type': 'application/json'
            },
            httpsAgent: new https.Agent({
                rejectUnauthorized: false, //< Disables certificate validation.
            })
        };

        logger.log("Request route= " + formatLogValue(route));
        logger.log("Request data= " + formatLogValue(data));
        logger.log("Request config= " + formatLogValue(config));

        try
        {
            const response = await axiosInstance.post(route, data, config);
            logger.log("Response= " + formatLogValue(response.data));
            appContext.integrationContext.integrationUserName = response.data.username;
            appContext.integrationContext.integrationPassword = response.data.password;
            appContext.integrationContext.integrationUserId = response.data.requestId;

            const integration_credentials = {
                integrationUserName: appContext.integrationContext.integrationUserName,
                integrationPassword: appContext.integrationContext.integrationPassword,
                integrationUserId: appContext.integrationContext.integrationUserId
            }

            logger.log("Writing integration_credentials.json data= " + formatLogValue(integration_credentials));
            fs.writeFileSync("integration_credentials.json", JSON.stringify(integration_credentials));
            fs.writeFileSync("integration_state.json", JSON.stringify({}));
            logger.log(
                `Integration request completed and credentials persisted. `
                + `integrationUserId=${appContext.integrationContext.integrationUserId}, `
                + `integrationUserName=${appContext.integrationContext.integrationUserName}`
            );
        }
        catch (error)
        {
            logger.error(`make-integration-request failed. error=${formatLogValue(error)}`);
        }
    });

    ipcMain.on("approve-integration-request", async () => {
        logger.log("IPC event: approve-integration-request.");

        if (!appContext.adminSessionToken)
        {
            logger.warn("approve-integration-request aborted: not logged in as admin.");
            return;
        }

        if (!appContext.integrationContext.integrationUserId)
        {
            logger.warn("approve-integration-request aborted: integration request does not exist.");
            return;
        }

        const integrationUserId = appContext.integrationContext.integrationUserId;
        const path = `rest/v4/analytics/integrations/*/requests/${integrationUserId}/approve`;
        const route = `https://${appContext.config.vmsHost}:${appContext.config.vmsPort}/${path}`;

        const data = {
            requestId: integrationUserId
        }

        const config = {
            headers: {
                'Content-Type': 'application/json',
                "Authorization": "Bearer " + appContext.adminSessionToken
            },
            httpsAgent: new https.Agent({
                rejectUnauthorized: false, //< Disables certificate validation.
            })
        };

        logger.log("Request route= " + formatLogValue(route));
        logger.log("Request config= " + formatLogValue(config));
        logger.log("Request data= " + formatLogValue(data));

        try
        {
            const response = await axiosInstance.post(route, data, config);
            logger.log("Response= " + formatLogValue(response.data));
            logger.log(`Integration request approved. integrationUserId=${integrationUserId}`);
        }
        catch (error)
        {
            logger.error(`approve-integration-request failed. error=${formatLogValue(error)}`);
        }
    });

    ipcMain.on("remove-integration", async () => {
        logger.log("IPC event: remove-integration.");

        if (!appContext.adminSessionToken)
        {
            logger.warn("remove-integration aborted: not logged in as admin.");
            return;
        }

        if (!appContext.integrationContext.integrationUserId)
        {
            logger.warn("remove-integration aborted: integration request id missing.");
            return;
        }

        const integrationUserId = appContext.integrationContext.integrationUserId;
        const usersPath = `rest/v4/users/${integrationUserId}`;
        const usersRoute = `https://${appContext.config.vmsHost}:${appContext.config.vmsPort}/${usersPath}`;

        const usersData = {
            id: integrationUserId
        };

        const usersConfig = {
            headers: {
                'Content-Type': 'application/json',
                "Authorization": "Bearer " + appContext.adminSessionToken
            },
            httpsAgent: new https.Agent({
                rejectUnauthorized: false, //< Disables certificate validation.
            }),
            data: usersData
        };

        let integrationId = null;

        try
        {
            logger.log("Request route= " + formatLogValue(usersRoute));
            logger.log("Request config= " + formatLogValue(usersConfig));

            const response = await axiosInstance.get(usersRoute, usersConfig);
            logger.log("Response= " + formatLogValue(response.data));

            integrationId = response.data.parameters.integrationRequestData.integrationId;
        }
        catch (error)
        {
            logger.error(`remove-integration failed while fetching user. error=${formatLogValue(error)}`);
            return;
        }

        const path = `rest/v4/analytics/integrations/${integrationId}`;
        const route = `https://${appContext.config.vmsHost}:${appContext.config.vmsPort}/${path}`;

        const data = {
            id: integrationId
        };

        const config = {
            headers: {
                'Content-Type': 'application/json',
                "Authorization": "Bearer " + appContext.adminSessionToken
            },
            httpsAgent: new https.Agent({
                rejectUnauthorized: false, //< Disables certificate validation.
            }),
            data: data
        };

        try
        {
            logger.log("Request route= " + formatLogValue(route));
            logger.log("Request data= " + formatLogValue(data));
            logger.log("Request config= " + formatLogValue(config));

            const response = await axiosInstance.delete(route, config);
            logger.log("Response= " + formatLogValue(response.data));

            appContext.integrationContext.integrationUserName = null;
            appContext.integrationContext.integrationPassword = null;
            appContext.integrationContext.integrationUserId = null;
        }
        catch (error)
        {
            logger.error(`remove-integration failed while deleting integration. error=${formatLogValue(error)}`);
            return
        }

        logger.log("Removing integration_credentials.json");

        fs.stat('./integration_credentials.json', function (err, stats)
        {
            logger.log("fs.stat result= " + formatLogValue(stats));

            if (err) return logger.error(`fs.stat failed error=${formatLogValue(err)}`);

            fs.unlink('./integration_credentials.json', function(err)
            {
                if (err) return logger.error(`fs.unlink failed error=${formatLogValue(err)}`);

                logger.log("integration_credentials.json deleted successfully.");
            });
        });

        logger.log("Removing integration_state.json");

        fs.stat('./integration_state.json', function (err, stats)
        {
            logger.log("fs.stat result= " + formatLogValue(stats));

            if (err) return logger.error(`fs.stat failed error=${formatLogValue(err)}`);

            fs.unlink('./integration_state.json', function(err)
            {
                if (err) return logger.error(`fs.unlink failed error=${formatLogValue(err)}`);

                logger.log("integration_state.json deleted successfully.");
            });
        });
    });

    ipcMain.on("update-timestamp", (event, timestampMs) => {
        logger.log(`IPC event: update-timestamp. timestampMs=${timestampMs}`);

        const nowMs = Date.now();
        const diagnostics = appContext.updateTimestampDiagnostics;
        if (diagnostics.lastUpdateWallclockMs != null)
        {
            const gapMs = nowMs - diagnostics.lastUpdateWallclockMs;
            if (gapMs >= kUpdateTimestampGapWarningMs)
            {
                logger.warn(
                    `update-timestamp gap detected. gapMs=${gapMs}, `
                    + `previousTimestampMs=${diagnostics.lastTimestampMs}, currentTimestampMs=${timestampMs}`
                );
            }
        }

        diagnostics.lastUpdateWallclockMs = nowMs;
        diagnostics.lastTimestampMs = timestampMs;
        diagnostics.eventCountInWindow += 1;

        const windowDurationMs = nowMs - diagnostics.windowStartMs;
        if (windowDurationMs >= kUpdateTimestampStatsIntervalMs)
        {
            const ratePerSec = (diagnostics.eventCountInWindow * 1000) / windowDurationMs;
            logger.log(
                `update-timestamp throughput. events=${diagnostics.eventCountInWindow}, `
                + `windowDurationMs=${windowDurationMs}, `
                + `ratePerSec=${Number(ratePerSec.toFixed(2))}, `
                + `lastTimestampMs=${diagnostics.lastTimestampMs}`
            );

            diagnostics.windowStartMs = nowMs;
            diagnostics.eventCountInWindow = 0;
        }

        let pushDataContext = appContext.integrationContext.deviceAgentPushDataContext;

        if (pushDataContext && pushDataContext.connected)
        {
            pushDataContext.pushDataInfo.timestampMs = timestampMs;
            pushDataContext.deviceAgent.pushData(pushDataContext.pushDataInfo);

            if (pushDataContext.frameCount > 50)
            {
                if (!pushDataContext.pdeFromEngine)
                {
                    pushDataContext.deviceAgent.pushPluginDiagnosticEvent("error", "Caption", "PDE from Device Agent");
                }
                else
                {
                    pushDataContext.engine.pushPluginDiagnosticEvent("error", "Caption", "PDE from Engine");
                }

                pushDataContext.pdeFromEngine = !pushDataContext.pdeFromEngine;
                pushDataContext.frameCount = 0;
            }

            pushDataContext.frameCount++;
        }
    });
}

app.whenReady().then(() => {
    logger.log("app.whenReady resolved.");
    initialize().then(() => {
        logger.log("Initialization completed. Registering notifications and creating window.");
        initializeNotifications();
        appContext.window = createWindow();
        logger.log("Main window assigned to appContext.");
    });

}).catch((exception) => {
    logger.error(`app.whenReady failed. error=${formatLogValue(exception)}`);
});
