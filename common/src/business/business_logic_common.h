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

namespace QnBusinessEventRuntime {
    static QLatin1String paramsKeyStr("paramsKey");

    inline QString getParamsKey(const QnBusinessParams &params) {
        return params[paramsKeyStr].toString();
    }

    inline void setParamsKey(QnBusinessParams* params, QString value) {
        (*params)[paramsKeyStr] = value;
    }
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
