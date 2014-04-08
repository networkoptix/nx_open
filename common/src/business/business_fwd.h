#ifndef QN_BUSINESS_FWD_H
#define QN_BUSINESS_FWD_H

#include <vector>

#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QMetaType>
#include <QtCore/QSharedPointer>

#undef PlaySound // TODO: #Elric who includes windows.h?

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

namespace QnBusiness {
    enum EventReason {
        NoReason,
        NetworkIssueNoFrame,
        NetworkIssueConnectionClosed,
        NetworkIssueRtpPacketLoss,
        MServerIssueTerminated,
        MServerIssueStarted,
        StorageIssueIoError,
        StorageIssueNotEnoughSpeed,
        StorageIssueNotEnoughSpace
    };
}
Q_DECLARE_METATYPE(QnBusiness::EventReason)

namespace BusinessEventType {
    enum Value {
        /** Event type is not defined. Used in rules. */
        NotDefined,

        /** Motion has occured on a camera. */
        Camera_Motion,

        /** Camera input signal is received. */
        Camera_Input,

        /** Camera was disconnected. */
        Camera_Disconnect,

        /** Storage read error has occured. */
        Storage_Failure,

        /** Network issue: packet lost, RTP timeout, etc. */
        Network_Issue,

        /** Found some cameras with same IP address. */
        Camera_Ip_Conflict,

        /** Connection to mediaserver lost. */
        MediaServer_Failure,

        /** Two or more mediaservers are running. */
        MediaServer_Conflict,

        /** Media server started */
        MediaServer_Started,

        /**
         * Used when enumerating to build GUI lists, this and followed actions
         * should not be displayed.
         */
        Count,

        /** System health message. */
        SystemHealthMessage = 500,

        /** Event group. */
        AnyCameraIssue = 600,
        AnyServerIssue = 601,
        AnyBusinessEvent = 602,

        /** Base index for the user defined events. */
        UserDefined = 1000
    };
}

namespace BusinessActionType {
    enum Value {
        NotDefined,

        //!change camera output state
        /*!
            parameters:\n
                - relayOutputID (string, required)          - id of output to trigger
                - relayAutoResetTimeout (uint, optional)    - timeout (in milliseconds) to reset camera state back
        */
        CameraOutput,

        Bookmark,           // mark part of camera archive as undeleted

        CameraRecording,    // start camera recording

        PanicRecording,     // activate panic recording mode
        // these actions can be executed from any endpoint. actually these actions call specified function at ec
        /*!
            parameters:\n
                - emailAddress (string, required)
        */
        SendMail,

        /**
         *  Write a record to the server's log
         */
        Diagnostics,

        ShowPopup,

        CameraOutputInstant,

        /*!
            parameters:\n
                - soundUrl (string, required)               - url of sound, contains path to sound on the EC
        */
        PlaySound,

        /*!
            parameters:\n
                - sayText (string, required)                - text that will be provided to TTS engine
        */
        SayText,

        PlaySoundRepeated,



        // media server based actions

        /**
         * Used when enumerating to build GUI lists, this and followed actions
         * should not be displayed.
         */
        Count
    };

    bool isNotImplemented(Value value);
}

#endif // QN_BUSINESS_FWD_H
