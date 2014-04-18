#ifndef QN_TIME_REPLY_H
#define QN_TIME_REPLY_H

#include <QtCore/QMetaType>

#include <utils/serialization/json_fwd.h>

struct QnTimeReply {
    /** Utc time in msecs since epoch. */
    qint64 utcTime;

    /** Time zone offset, in msecs. */
    qint64 timeZoneOffset;
};

Q_DECLARE_METATYPE(QnTimeReply)
QN_DECLARE_JSON_SERIALIZATION_FUNCTIONS(QnTimeReply)

#endif // QN_TIME_REPLY_H
