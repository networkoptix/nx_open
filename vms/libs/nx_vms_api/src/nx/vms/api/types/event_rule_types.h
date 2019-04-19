#pragma once

#include <vector>

#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QMetaType>
#include <QtCore/QSharedPointer>

#include <nx/vms/api/types_fwd.h>

namespace nx {
namespace vms {

// Namespace api.
#ifdef THIS_BLOCK_IS_REQUIRED_TO_MAKE_FILE_BE_PROCESSED_BY_MOC_DO_NOT_DELETE
Q_OBJECT
#endif
QN_DECLARE_METAOBJECT_HEADER(api, EventReason EventState EventType ActionType, )

enum class EventReason
{
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

    // Server Failure event
    serverTerminated = 4,
    serverStarted = 5,

    // Storage Failure event
    storageIoError = 6,
    storageTooSlow = 7,
    storageFull = 8,
    systemStorageFull = 9,

    // License Issue event
    licenseRemoved = 10,

    // Backup Finished event
    backupFailedNoBackupStorageError = 11,
    backupFailedSourceStorageError = 12,
    backupFailedSourceFileError = 13,
    backupFailedTargetFileError = 14,
    backupFailedChunkError = 15,
    backupEndOfPeriod = 16,
    backupDone = 17,
    backupCancelled = 18,

    // last number is 23, see networkMulticastAddressIsInvalid
};
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(EventReason)

enum class EventState
{
    inactive = 0,
    active = 1,

    /** Also used in event rule to associate non-toggle action with event with any toggle state. */
    undefined = 2
};
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(EventState)

enum EventType
{
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
    pluginEvent = 14,

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
};
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(EventType)

enum ActionType
{
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
};
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(ActionType)

enum EventLevel
{
    UndefinedEventLevel = 0,
    InfoEventLevel = 1 << 0,
    WarningEventLevel = 1 << 1,
    ErrorEventLevel = 1 << 2,
};
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(EventLevel)

Q_DECLARE_FLAGS(EventLevels, EventLevel)
Q_DECLARE_OPERATORS_FOR_FLAGS(EventLevels)

} // namespace api
} // namespace vms
} // namespace nx

QN_FUSION_DECLARE_FUNCTIONS(nx::vms::api::EventType, (metatype)(lexical)(debug), NX_VMS_API)
QN_FUSION_DECLARE_FUNCTIONS(nx::vms::api::ActionType, (metatype)(lexical)(debug), NX_VMS_API)

QN_FUSION_DECLARE_FUNCTIONS(nx::vms::api::EventLevel, (metatype)(numeric)(lexical)(debug), NX_VMS_API)
QN_FUSION_DECLARE_FUNCTIONS(nx::vms::api::EventLevels, (metatype)(numeric)(lexical)(debug), NX_VMS_API)
