#ifndef QN_BUSINESS_FWD_H
#define QN_BUSINESS_FWD_H

#include <vector>

#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QMetaType>
#include <QtCore/QSharedPointer>

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

#ifdef Q_MOC_RUN
class QnBusiness
#else
namespace QnBusiness
#endif
{
#ifdef Q_MOC_RUN
    Q_GADGET
        Q_ENUMS(EventReason EventType ActionType)
public:
#else
    Q_NAMESPACE
#endif

    enum EventReason {
        NoReason,
        NetworkNoFrameReason,
        NetworkConnectionClosedReason,
        NetworkRtpPacketLossReason,
        ServerTerminatedReason,
        ServerStartedReason,
        StorageIoErrorReason,
        StorageTooSlowReason,
        StorageNotEnoughSpaceReason
    };

    enum EventType {
        /** Event type is not defined. Used in rules. */
        UndefinedEvent,

        /** Motion has occured on a camera. */
        CameraMotionEvent,

        /** Camera input signal is received. */
        CameraInputEvent,

        /** Camera was disconnected. */
        CameraDisconnectEvent,

        /** Storage read error has occured. */
        StorageFailureEvent,

        /** Network issue: packet lost, RTP timeout, etc. */
        NetworkIssueEvent,

        /** Found some cameras with same IP address. */
        CameraIpConflictEvent,

        /** Connection to mediaserver lost. */
        ServerFailureEvent,

        /** Two or more mediaservers are running. */
        ServerConflictEvent,

        /** Media server started */
        ServerStartEvent,

        /**
         * Used when enumerating to build GUI lists, this and followed actions
         * should not be displayed.
         */
        EventCount, // TODO: #Elric remove

        /** System health message. */
        SystemHealthEvent = 500,

        /** Event group. */
        AnyCameraEvent = 600,
        AnyServerEvent = 601,
        AnyBusinessEvent = 602,

        /** Base index for the user defined events. */
        UserEvent = 1000
    };

    enum ActionType {
        UndefinedAction,

        /** Change camera output state.
         *
         * Parameters:
         * - relayOutputID (string, required)          - id of output to trigger.
         * - relayAutoResetTimeout (uint, optional)    - timeout (in milliseconds) to reset camera state back.
         */
        CameraOutputAction,

        CameraOutputOnceAction,

        BookmarkAction,

        /** Start camera recording. */
        CameraRecordingAction,

        /** Activate panic recording mode. */
        PanicRecordingAction,     

        /** 
         * Send an email. This action can be executed from any endpoint. 
         *
         * Parameters:
         * - emailAddress (string, required)
         */
        SendMailAction,

        /** Write a record to the server's log. */
        DiagnosticsAction,

        ShowPopupAction,

        /**
         * Parameters:
         * - soundUrl (string, required)               - url of sound, contains path to sound on the EC.
         */
        PlaySoundAction,

        PlaySoundOnceAction,


        /**
         * Parameters:
         * - sayText (string, required)                - text that will be provided to TTS engine.
         */
        SayTextAction,

        /**
         * Used when enumerating to build GUI lists, this and followed actions
         * should not be displayed.
         */
        ActionCount // TODO: #Elric remove
    };

    bool isNotImplemented(ActionType actionType);

} // namespace QnBusiness

Q_DECLARE_METATYPE(QnBusiness::EventReason)
Q_DECLARE_METATYPE(QnBusiness::EventType)
Q_DECLARE_METATYPE(QnBusiness::ActionType)


#endif // QN_BUSINESS_FWD_H
