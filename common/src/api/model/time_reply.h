#ifndef QN_TIME_REPLY_H
#define QN_TIME_REPLY_H

#include <QtCore/QMetaType>

#include <utils/common/model_functions_fwd.h>

struct QnTimeReply {
    /** Utc time in msecs since epoch. */
    qint64 utcTime;

    /** Time zone offset, in msecs. */
    qint64 timeZoneOffset;
};

#define QnTimeReply_Fields (utcTime)(timeZoneOffset)

QN_FUSION_DECLARE_FUNCTIONS(QnTimeReply, (json)(metatype))

#endif // QN_TIME_REPLY_H
