#ifndef QN_API_FWD_H
#define QN_API_FWD_H

#include <QtCore/QSharedPointer>
#include <QtCore/QList>
#include <QtCore/QMap>

class QnUuid;

class QnAppServerConnection;
typedef QSharedPointer<QnAppServerConnection> QnAppServerConnectionPtr;

class QnMediaServerConnection;
typedef QSharedPointer<QnMediaServerConnection> QnMediaServerConnectionPtr;

struct QnChunksRequestData;

struct MultiServerPeriodData;
typedef std::vector<MultiServerPeriodData> MultiServerPeriodDataList;

#endif // QN_API_FWD_H
