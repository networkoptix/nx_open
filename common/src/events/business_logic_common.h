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
typedef QMap<QString, QVariant> QnBusinessParams; // param name and param value


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
