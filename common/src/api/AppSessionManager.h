#ifndef _APP_SESSION_MANAGER_H
#define _APP_SESSION_MANAGER_H

#include <QtCore/QSharedPointer>

#include "utils/network/simple_http_client.h"
#include "utils/common/qnid.h"

#include "QStdIStream.h"

#include "SessionManager.h"

class AppSessionManager : public SessionManager
{
    Q_OBJECT

public:
    AppSessionManager(const QUrl &url);

    int addObject(const QString& objectName, const QByteArray& body, QByteArray& response, QByteArray& errorString);
    int addObjectAsync(const QString& objectName, const QByteArray& data, QObject* target, const char* slot);

    int getObjects(const QString& objectName, const QString& args, QByteArray& data,
                   QByteArray& errorString);
};

#endif // _APP_SESSION_MANAGER_H
