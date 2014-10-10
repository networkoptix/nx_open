#ifndef QN_BUSINESS_FWD_H
#define QN_BUSINESS_FWD_H

#include <vector>

#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QMetaType>
#include <QtCore/QSharedPointer>

#include <utils/common/model_functions_fwd.h>

class QnAbstractBusinessEvent;
typedef QSharedPointer<QnAbstractBusinessEvent> QnAbstractBusinessEventPtr;
typedef QList<QnAbstractBusinessEventPtr> QnAbstractBusinessEventList;

class QnAbstractBusinessAction;
typedef QSharedPointer<QnAbstractBusinessAction> QnAbstractBusinessActionPtr;
typedef QList<QnAbstractBusinessActionPtr> QnAbstractBusinessActionList;

class QnPanicBusinessAction;
typedef QSharedPointer<QnPanicBusinessAction> QnPanicBusinessActionPtr;
typedef QList<QnPanicBusinessAction> QnPanicBusinessActionList;

class QnBusinessActionData;
typedef std::vector<QnBusinessActionData> QnBusinessActionDataList;
typedef QSharedPointer<QnBusinessActionDataList> QnBusinessActionDataListPtr;

typedef QMap<QString, QVariant> QnBusinessParams;

class QnBusinessEventRule;
typedef QSharedPointer<QnBusinessEventRule> QnBusinessEventRulePtr;
typedef QList<QnBusinessEventRulePtr> QnBusinessEventRuleList;

struct QnCameraConflictList;

#ifdef Q_MOC_RUN
class QnBusiness
#else
namespace QnBusiness
#endif
{
#ifdef Q_MOC_RUN
    Q_GADGET
    Q_ENUMS(EventReason EventState EventType ActionType)
public:
#else
    Q_NAMESPACE
#endif

    enum EventReason {
        NoReason = 0,
        NetworkNoFrameReason,
        NetworkConnectionClosedReason,
        NetworkRtpPacketLossReason,
        ServerTerminatedReason,
        ServerStartedReason,
        StorageIoErrorReason,
        StorageTooSlowReason,
        StorageFullReason,
        LicenseRemoved
    };

    enum EventState {
        InactiveState = 0,
        ActiveState = 1,

        /** Also used in event rule to associate non-toggle action with event with any toggle state. */
        UndefinedState = 2
    };
    QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(EventState)

    enum EventType {
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

        /** Connection to mediaserver lost. */
        ServerFailureEvent = 7,

        /** Two or more mediaservers are running. */
        ServerConflictEvent = 8,

        /** Server started */
        ServerStartEvent = 9,
        
        /** Not enough licenses */
        LicenseIssueEvent = 10,

        /** System health message. */
        SystemHealthEvent = 500,

        /** Event group. */
        AnyCameraEvent = 600,
        AnyServerEvent = 601,
        AnyBusinessEvent = 602,

        /** Base index for the user defined events. */
        UserEvent = 1000
    };
    QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(EventType)

    enum ActionType {
        UndefinedAction = 0,

        /** 
         * Change camera output state.
         *
         * Parameters:
         * - relayOutputID (string, required)          - id of output to trigger.
         * - relayAutoResetTimeout (uint, optional)    - timeout (in milliseconds) to reset camera state back.
         */
        CameraOutputAction = 1,

        CameraOutputOnceAction = 2,

        BookmarkAction = 3,

        /** Start camera recording. */
        CameraRecordingAction = 4,

        /** Activate panic recording mode. */
        PanicRecordingAction = 5,

        /** 
         * Send an email. This action can be executed from any endpoint. 
         *
         * Parameters:
         * - emailAddress (string, required)
         */
        SendMailAction = 6,

        /** Write a record to the server's log. */
        DiagnosticsAction = 7,

        ShowPopupAction = 8,

        /**
         * Parameters:
         * - soundUrl (string, required)               - url of sound, contains path to sound on the Server.
         */
        PlaySoundAction = 9,

        PlaySoundOnceAction = 10,

        /**
         * Parameters:
         * - sayText (string, required)                - text that will be provided to TTS engine.
         */
        SayTextAction = 11,

        /**
         * Used when enumerating to build GUI lists, this and followed actions
         * should not be displayed.
         */
        ActionCount // TODO: #Elric remove
    };
    QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(ActionType)

} // namespace QnBusiness

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((QnBusiness::EventReason)(QnBusiness::EventState)(QnBusiness::EventType)(QnBusiness::ActionType), (metatype)(lexical))

#endif // QN_BUSINESS_FWD_H
