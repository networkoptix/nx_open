#ifndef QN_TIME_REPLY_H
#define QN_TIME_REPLY_H

#include <QtCore/QMetaType>

#include <utils/common/json.h>

struct QnTimeReply {
    /** Utc time in msecs since epoch. */
    qint64 utcTime;

    /** Time zone offset, in msecs. */
    qint64 timeZoneOffset;
};

QN_DEFINE_STRUCT_SERIALIZATION_FUNCTIONS(QnTimeReply, (utcTime)(timeZoneOffset), inline)

Q_DECLARE_METATYPE(QnTimeReply)

#endif // QN_TIME_REPLY_H
