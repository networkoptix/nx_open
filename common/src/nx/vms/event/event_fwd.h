#pragma once

#include <vector>

#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QMetaType>
#include <QtCore/QSharedPointer>

#include <common/common_globals.h>

#include <nx/fusion/model_functions_fwd.h>

// TODO: #vkutin Think of a location to put this to
struct QnCameraConflictList;

namespace nx {
namespace vms {
namespace event {

class AbstractEvent;
typedef QSharedPointer<AbstractEvent> AbstractEventPtr;
typedef QList<AbstractEventPtr> AbstractEventList;

class AbstractAction;
typedef QSharedPointer<AbstractAction> AbstractActionPtr;
typedef QList<AbstractActionPtr> AbstractActionList;

class PanicAction;
typedef QSharedPointer<PanicAction> PanicActionPtr;
typedef QList<PanicAction> PanicActionList;

class ActionData;
typedef std::vector<ActionData> ActionDataList;
typedef QSharedPointer<ActionDataList> ActionDataListPtr;

typedef QMap<QString, QVariant> QnBusinessParams;

class Rule;
typedef QSharedPointer<Rule> RulePtr;
typedef QList<RulePtr> RuleList;

struct EventParameters;
struct ActionParameters;

} // namespace event

#ifdef THIS_BLOCK_IS_REQUIRED_TO_MAKE_FILE_BE_PROCESSED_BY_MOC_DO_NOT_DELETE
Q_OBJECT
#endif
QN_DECLARE_METAOBJECT_HEADER(event, EventReason EventState EventType ActionType, )

enum class EventReason
{
    none = 0,
    networkNoFrame,
    networkConnectionClosed,
    networkRtpPacketLoss,
    serverTerminated,
    serverStarted,
    storageIoError,
    storageTooSlow,
    storageFull,
    systemStorageFull,
    licenseRemoved,
    backupFailedNoBackupStorageError,
    backupFailedSourceStorageError,
    backupFailedSourceFileError,
    backupFailedTargetFileError,
    backupFailedChunkError,
    backupEndOfPeriod,
    backupDone,
    backupCancelled,
    networkNoResponseFromDevice
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
     * Show text overlay over the given camera(s).
     * actionParams:
     * - text (string, required) - Text that will be displayed.
     */
    showTextOverlayAction = 13,

    /**
     * Put the given camera(s) to the Alarm Layout.
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

    acknowledgeAction = 16
};
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(ActionType)

} // namespace event
} // namespace vms
} // namespace nx

#define QN_VMS_EVENT_ENUM_TYPES \
    (nx::vms::event::EventReason) \
    (nx::vms::event::EventType) \
    (nx::vms::event::ActionType) \
    (nx::vms::event::EventState)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(QN_VMS_EVENT_ENUM_TYPES, (metatype)(lexical))
