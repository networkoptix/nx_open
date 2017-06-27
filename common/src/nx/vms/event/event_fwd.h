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

enum EventReason
{
    NoReason = 0,
    NetworkNoFrameReason,
    NetworkConnectionClosedReason,
    NetworkRtpPacketLossReason,
    ServerTerminatedReason,
    ServerStartedReason,
    StorageIoErrorReason,
    StorageTooSlowReason,
    StorageFullReason,
    SystemStorageFullReason,
    LicenseRemoved,
    BackupFailedNoBackupStorageError,
    BackupFailedSourceStorageError,
    BackupFailedSourceFileError,
    BackupFailedTargetFileError,
    BackupFailedChunkError,
    BackupEndOfPeriod,
    BackupDone,
    BackupCancelled,

    NetworkNoResponseFromDevice
};
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(EventReason)

enum EventState
{
    InactiveState = 0,
    ActiveState = 1,

    /** Also used in event rule to associate non-toggle action with event with any toggle state. */
    UndefinedState = 2
};
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(EventState)

enum EventType
{
    /** Event type is not defined. Used in rules. */
    UndefinedEvent = 0,

    /** Motion has occurred on a camera. */
    CameraMotionEvent = 1,

    /** Camera input signal is received. */
    CameraInputEvent = 2,

    /** Camera was disconnected. */
    CameraDisconnectEvent = 3,

    /** Storage read error has occurred. */
    StorageFailureEvent = 4,

    /** Network issue: packet lost, RTP timeout, etc. */
    NetworkIssueEvent = 5,

    /** Found some cameras with same IP address. */
    CameraIpConflictEvent = 6,

    /** Connection to server lost. */
    ServerFailureEvent = 7,

    /** Two or more servers are running. */
    ServerConflictEvent = 8,

    /** Server started. */
    ServerStartEvent = 9,

    /** Not enough licenses. */
    LicenseIssueEvent = 10,

    /** Archive backup done. */
    BackupFinishedEvent = 11,

    /** Software triggers. */
    SoftwareTriggerEvent = 12,

    /** System health message. */
    SystemHealthEvent = 500,

    /** System health message. */
    MaxSystemHealthEvent = 599,

    /** Event group. */
    AnyCameraEvent = 600,

    /** Event group. */
    AnyServerEvent = 601,

    /** Event group. */
    AnyEvent = 602,

    /** Base index for the user defined events. */
    UserDefinedEvent = 1000
};
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(EventType)

enum ActionType
{
    UndefinedAction         = 0,

    /**
        * Change camera output state.
        *
        * Parameters:
        * - relayOutputID (string, required)          - id of output to trigger.
        * - durationMs (uint, optional)               - timeout (in milliseconds) to reset camera state back.
        */
    CameraOutputAction      = 1,

    BookmarkAction          = 3,

    /** Start camera recording. */
    CameraRecordingAction   = 4,

    /** Activate panic recording mode. */
    PanicRecordingAction    = 5,

    /**
        * Send an email. This action can be executed from any endpoint.
        *
        * Parameters:
        * - emailAddress (string, required)
        */
    SendMailAction          = 6,

    /** Write a record to the server's log. */
    DiagnosticsAction       = 7,

    ShowPopupAction         = 8,

    /**
        * Parameters:
        * - url (string, required)                    - url of sound, contains path to sound on the Server.
        */
    PlaySoundAction         = 9,
    PlaySoundOnceAction     = 10,

    /**
        * Parameters:
        * - sayText (string, required)                - text that will be provided to TTS engine.
        */
    SayTextAction           = 11,

    /**
        * Execute given PTZ preset.
        * Parameters:
        * (resourceId, presetId)
        */
    ExecutePtzPresetAction  = 12,

    /**
        * Show text overlay over the given camera(s).
        * Parameters:
        * - text (string, required)                    - text that will be displayed.
        */
    ShowTextOverlayAction   = 13,

    /**
        * Put the given camera(s) to the Alarm Layout.
        * Parameters:
        * - users                                      - list of users, which will receive this alarm notification
        */
    ShowOnAlarmLayoutAction = 14,

    /**
        * Send HTTP request as an action.
        * Parameters:
        * - url                                        - full HTTP url to execute. username/password are stored as part of the URL
        * - text                                       - HTTP message body for POST method
        */
    ExecHttpRequestAction = 15
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
