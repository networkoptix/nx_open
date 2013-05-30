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

class QnBusinessActionData;
typedef std::vector<QnBusinessActionData> QnBusinessActionDataList;
typedef QSharedPointer<QnBusinessActionDataList> QnBusinessActionDataListPtr;

typedef QMap<QString, QVariant> QnBusinessParams;

namespace QnBusiness {
    enum EventReason {
        NoReason,
        NetworkIssueNoFrame,
        NetworkIssueConnectionClosed,
        NetworkIssueRtpPacketLoss,
        MServerIssueTerminated,
        MServerIssueStarted,
        StorageIssueIoError,
        StorageIssueNotEnoughSpeed
    };
}
Q_DECLARE_METATYPE(QnBusiness::EventReason);

namespace BusinessEventType {
    enum Value {
        /** Motion has occured on a camera. */
        Camera_Motion,

        /** Camera was disconnected. */
        Camera_Disconnect,

        /** Storage read error has occured. */
        Storage_Failure,

        /** Network issue: packet lost, RTP timeout, etc. */
        Network_Issue,

        /** Found some cameras with same IP address. */
        Camera_Ip_Conflict,

        /** Camera input signal is received. */
        Camera_Input,

        /** Connection to mediaserver lost. */
        MediaServer_Failure,

        /** Two or more mediaservers are running. */
        MediaServer_Conflict,

        /** Event type is not defined. Used in rules. */
        NotDefined,

        /**
         * Used when enumerating to build GUI lists, this and followed actions
         * should not be displayed.
         */
        Count = NotDefined,

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
        CameraRecording,    // start camera recording
        PanicRecording,     // activate panic recording mode
        // these actions can be executed from any endpoint. actually these actions call specified function at ec
        /*!
            parameters:\n
                - emailAddress (string, required)
        */
        SendMail,

        ShowPopup,

        //!change camera output state
        /*!
            parameters:\n
                - relayOutputID (string, required)          - id of output to trigger
                - relayAutoResetTimeout (uint, optional)    - timeout (in milliseconds) to reset camera state back
        */
        CameraOutput,
        CameraOutputInstant,

        /*!
            parameters:\n
                - soundSource (int, required)               - enumeration describing source of the sound (resources, EC, TTS)
                - soundUrl (string, required)               - url of sound, can contain:
                                                                * path to sound on the EC
                                                                * path to the resource
                                                                * text that will be provided to TTS engine
        */
        PlaySound,

        Alert,              // write a record to the server's log
        Bookmark,           // mark part of camera archive as undeleted

        // media server based actions
        NotDefined,

        /**
         * Used when enumerating to build GUI lists, this and followed actions
         * should not be displayed.
         */
        Count = Alert
    };
}

#endif // QN_BUSINESS_FWD_H
