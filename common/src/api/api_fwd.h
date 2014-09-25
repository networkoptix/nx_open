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

// TODO: #Elric bad naming
class QnKvPair;
typedef QList<QnKvPair> QnKvPairList;
typedef QMap<QnUuid, QnKvPairList> QnKvPairListsById;


#endif // QN_API_FWD_H
