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

typedef QList<QPair<QString, bool> > QnStringBoolPairList;
typedef QList<QPair<QString, QVariant> > QnStringVariantPairList;

Q_DECLARE_METATYPE(QnStringBoolPairList);
Q_DECLARE_METATYPE(QnStringVariantPairList);

#endif // QN_API_FWD_H
