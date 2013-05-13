#ifndef QN_BUSINESS_LOGIC_COMMON_H
#define QN_BUSINESS_LOGIC_COMMON_H

#include <QtCore/QString>
#include <QtCore/QVariant>

#include <utils/common/json.h>

namespace ToggleState
{
    enum Value
    {
        Off,
        On,
        // Also Used in event rule to associate non-toggle action with event with any toggle state
        NotDefined

    };
}

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

typedef QMap<QString, QVariant> QnBusinessParams; // param name and param value


namespace BusinessEventType
{
    enum Value
    {
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

        /** Base index for the user defined events. */
        UserDefined = 1000

    };
}

inline QByteArray serializeBusinessParams(const QnBusinessParams& value) {
    QByteArray result;
    QJson::serialize(value, &result);
    return result;
}

inline QnBusinessParams deserializeBusinessParams(const QByteArray& value) {
    QnBusinessParams result;
    QJson::deserialize(value, &result);
    return result; /* Returns empty map in case of deserialization error. */
}



#endif // QN_BUSINESS_LOGIC_COMMON_H
