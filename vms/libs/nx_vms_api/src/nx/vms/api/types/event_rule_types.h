// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/reflect/enum_instrument.h>
#include <nx/utils/serialization/flags.h>

namespace nx::vms::api {

NX_REFLECTION_ENUM_CLASS(EventReason,
    none = 0,

    // Network Issue event
    networkNoFrame = 1,
    networkConnectionClosed = 2,
    networkRtpPacketLoss = 3,
    networkNoResponseFromDevice = 19,
    networkBadCameraTime = 20,
    networkCameraTimeBackToNormal = 21,
    networkMulticastAddressConflict = 22,
    networkMulticastAddressIsInvalid = 23,
    networkRtpStreamError = 24,

    // Server Failure event
    serverTerminated = 4,
    serverStarted = 5,

    // Storage Failure event
    storageIoError = 6,
    storageTooSlow = 7,
    storageFull = 8,
    systemStorageFull = 9,
    metadataStorageOffline = 1010,
    metadataStorageFull = 1011,
    raidStorageError = 1012,
    metadataStoragePermissionDenied = 1013,
    encryptionFailed = 1014,

    // License Issue event
    licenseRemoved = 10,

    // LDAP Sync Issue event
    failedToConnectToLdap = 25,
    failedToCompleteSyncWithLdap = 26,
    noLdapUsersAfterSync = 27,
    someUsersNotFoundInLdap = 28,

    // Backup Finished event & Storage Failure event
    backupFailedSourceFileError = 13

    // Last number is 28, see someUsersNotFoundInLdap. Some numbers are 1000+.
)

enum class EventState
{
    /**%apidoc
     * %caption Inactive
     */
    inactive = 0,

    /**%apidoc
     * %caption Active
     */
    active = 1,

    /**%apidoc
     * Also used in event rule to associate non-toggle action with event with any toggle state.
     * %caption Undefined
     */
    undefined = 2
};

template<typename Visitor>
constexpr auto nxReflectVisitAllEnumItems(EventState*, Visitor&& visitor)
{
    using Item = nx::reflect::enumeration::Item<EventState>;
    return visitor(
        Item{EventState::inactive, "Inactive"},
        Item{EventState::active, "Active"},
        Item{EventState::undefined, "Undefined"}
    );
}

NX_REFLECTION_ENUM(EventType,
    /** Event type is not defined. Used in rules. */
    undefinedEvent = 0,

    /** Motion has occurred on a camera. */
    cameraMotionEvent = 1,

    /** Camera input signal is received. */
    cameraInputEvent = 2,

    /** Camera was disconnected. */
    cameraDisconnectEvent = 3,

    /** Storage read error has occurred. */
    storageFailureEvent = 4,

    /** Network issue: packet lost, RTP timeout, etc. */
    networkIssueEvent = 5,

    /** Found some cameras with same IP address. */
    cameraIpConflictEvent = 6,

    /** Connection to server lost. */
    serverFailureEvent = 7,

    /** Two or more servers are running. */
    serverConflictEvent = 8,

    /** Server started. */
    serverStartEvent = 9,

    /** Not enough licenses. */
    licenseIssueEvent = 10,

    /** Archive backup done. */
    backupFinishedEvent = 11,

    /** Software triggers. */
    softwareTriggerEvent = 12,

    /** Analytics SDK. */
    analyticsSdkEvent = 13,

    /** Plugin events. */
    pluginDiagnosticEvent = 14,

    /** NVR PoE over-budget event. */
    poeOverBudgetEvent = 15,

    /** NVR cooling fan error event. */
    fanErrorEvent = 16,

    /** Analytics SDK object detected. */
    analyticsSdkObjectDetected = 17,

    /** A Server handshake certificate mismatched the certificate saved in the System DB. */
    serverCertificateError = 18,

    /** LDAP sync issue. */
    ldapSyncIssueEvent = 19,

    /** System health message. */
    systemHealthEvent = 500,

    /** System health message. */
    maxSystemHealthEvent = 599,

    /** Event group. */
    anyCameraEvent = 600,

    /** Event group. */
    anyServerEvent = 601,

    /** Event group. */
    anyEvent = 602,

    /** Base index for the user defined events. */
    userDefinedEvent = 1000
)

NX_REFLECTION_ENUM(ActionType,
    undefinedAction = 0,

    /**
     * Change camera output state.
     * actionParams:
     * - relayOutputID (string, required)          - id of output to trigger.
     * - durationMs (uint, optional)               - timeout (in milliseconds) to reset camera state back.
     */
    cameraOutputAction = 1,

    bookmarkAction = 3,

    /** Start camera recording. */
    cameraRecordingAction = 4,

    /** Activate panic recording mode. */
    panicRecordingAction = 5,

    /**
     * Send an email. This action can be executed from any endpoint.
     * actionParams:
     * - emailAddress (string, required)
     */
    sendMailAction = 6,

    /** Write a record to the server log. */
    diagnosticsAction = 7,

    showPopupAction = 8,

    /**
     * actionParams:
     * - url (string, required) - Url of the sound, contains path to the sound on the server.
     */
    playSoundAction = 9,

    /**
     * actionParams:
     * - url (string, required) - Url of the sound, contains path to the sound on the server.
     */
    playSoundOnceAction = 10,

    /**
     * actionParams:
     * - sayText (string, required) - Text that will be provided to TTS engine.
     */
    sayTextAction = 11,

    /**
     * Execute given PTZ preset.
     * actionParams:
     * - resourceId
     * - presetId
     */
    executePtzPresetAction = 12,

    /**
     * Show text overlay over the given cameras.
     * actionParams:
     * - text (string, required) - Text that will be displayed.
     */
    showTextOverlayAction = 13,

    /**
     * Put the given cameras to the Alarm Layout.
     * actionParams:
     * - users - List of users which will receive this alarm notification.
     */
    showOnAlarmLayoutAction = 14,

    /**
     * Send HTTP request as an action.
     * actionParams:
     * - url - Full HTTP url to execute. Username/password are stored as part of the URL.
     * - text - HTTP message body for POST method.
     */
    execHttpRequestAction = 15,

    /**
     * Displays event notification for the user, where special 'Acknowledge' button is available.
     * On this button press, bookmark on the source camera is created.
     */
    acknowledgeAction = 16,

    /**
     * Expand given camera to fullscreen if it is displayed on the current layout.
     * Parameters:
     * - camera (may be taken from the event)
     * - layout (may belong to the fixed user or be shared)
     */
    fullscreenCameraAction = 17,

    /**
     * Reset given layout from fullscreen if it is displayed currently.
     * Parameters:
     * - layout (may belong to the fixed user or be shared)
     */
    exitFullscreenAction = 18,

    /**
    * Open layout as an action.
    * actionParams:
    * - layoutResourceId - Uuid of layout to be opened
    */
    openLayoutAction = 19,

    /**
     * Enable a buzzer on an NVR.
     * Parameters:
     * - serverIds (may be taken from event parameters),
     * - durationMs (optional)
     */
    buzzerAction = 20,

    /** Send push notification using cloud. */
    pushNotificationAction = 21,

    /**
     * Open intercom as an action. TODO: #dfisenko Describe the action when it is defined.
     * actionParams:
     * TODO: #dfisenko Add parameters when they are defined.
     */
    showIntercomInformer = 22
)

enum class EventLevel
{
    /**%apidoc[unused] */
    undefined = 0,

    info = 1 << 0,
    warning = 1 << 1,
    error = 1 << 2,
};

template<typename Visitor>
constexpr auto nxReflectVisitAllEnumItems(EventLevel*, Visitor&& visitor)
{
    using Item = nx::reflect::enumeration::Item<EventLevel>;
    return visitor(
        Item{EventLevel::undefined, ""},
        Item{EventLevel::info, "info"},
        Item{EventLevel::warning, "warning"},
        Item{EventLevel::error, "error"}
    );
}

Q_DECLARE_FLAGS(EventLevels, EventLevel)
Q_DECLARE_OPERATORS_FOR_FLAGS(EventLevels)

} // namespace nx::vms::api
