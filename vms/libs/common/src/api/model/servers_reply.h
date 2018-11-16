#ifndef QN_SERVERS_REPLY_H
#define QN_SERVERS_REPLY_H

#include <QtCore/QByteArray>
#include <QtCore/QMetaType>

#include <core/resource/resource_fwd.h>

struct QnServersReply {
    QnMediaServerResourceList servers;
    QByteArray authKey;
};
Q_DECLARE_METATYPE(QnServersReply)

#endif // QN_SERVERS_REPLY_H
