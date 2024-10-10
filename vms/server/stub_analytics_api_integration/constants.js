// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

"use strict"

module.exports = {
    // Server to Integration requests.

    GET_ENGINE_COMPATIBILITY_INFO_METHOD: "rest.v4.analytics.engines.compatibilityInfo.get",
    CREATE_DEVICE_AGENT_METHOD: "rest.v4.analytics.engines.deviceAgents.create",
    SET_ENGINE_SETTINGS_METHOD: "rest.v4.analytics.engines.settings.update",
    SET_DEVICE_AGENT_SETTINGS_METHOD: "rest.v4.analytics.engines.deviceAgents.settings.update",
    GET_ENGINE_SETTINGS_ON_ACTIVE_SETTING_CHANGE:
        "rest.v4.analytics.engines.settings.notifyActiveSettingChanged",
    GET_DEVICE_AGENT_SETTINGS_ON_ACTIVE_SETTING_CHANGE:
        "rest.v4.analytics.engines.deviceAgents.settings.notifyActiveSettingChanged",
    EXECUTE_ENGINE_ACTION: "rest.v4.analytics.engines.executeAction",
    GET_ENGINE_SIDE_SETTINGS_METHOD: "rest.v4.analytics.engines.integrationSideSettings.get",
    GET_DEVICE_AGENT_SIDE_SETTINGS_METHOD:
        "rest.v4.analytics.engines.deviceAgents.integrationSideSettings.get",

    // Server to Integration notifications.

    RECEIVE_DEVICE_AGENT_DATA_METHOD: "rest.v4.analytics.engines.deviceAgents.data.create",
    DELETE_DEVICE_AGENT_METHOD: "rest.v4.analytics.engines.deviceAgents.delete",

    // Integration to Server notifications.

    SUBSCRIBE: "rest.v4.analytics.subscribe",

    PUSH_DEVICE_AGENT_OBJECT_METADATA_METHOD:
        "rest.v4.analytics.engines.deviceAgents.metadata.object.create",
    PUSH_DEVICE_AGENT_EVENT_METADATA_METHOD:
        "rest.v4.analytics.engines.deviceAgents.metadata.event.create",
    PUSH_DEVICE_AGENT_BESTSHOT_METADATA_METHOD:
        "rest.v4.analytics.engines.deviceAgents.metadata.bestShot.create",
    PUSH_DEVICE_AGENT_OBJECT_TITLE_METADATA_METHOD:
        "rest.v4.analytics.engines.deviceAgents.metadata.title.create",
    PUSH_DEVICE_AGENT_MANIFEST_METHOD:
        "rest.v4.analytics.engines.deviceAgents.manifest.create",
    PUSH_ENGINE_MANIFEST_METHOD: "rest.v4.analytics.engines.manifest.create",
    PUSH_ENGINE_PLUGIN_DIAGNOSTIC_EVENT_METHOD:
        "rest.v4.analytics.engines.integrationDiagnosticEvent.create",
    PUSH_DEVICE_AGENT_PLUGIN_DIAGNOSTIC_EVENT_METHOD:
        "rest.v4.analytics.engines.deviceAgents.integrationDiagnosticEvent.create",

    // JSON-RPC subscription methods.
    GET_USER_METHOD: "rest.v4.users.get",
};
