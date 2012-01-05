#ifndef APPSERVERCONNECTIONIMPL_H
#define APPSERVERCONNECTIONIMPL_H

#include <QtCore/QMutex>
#include <QtCore/QUrl>

#include "core/resource/resource_type.h"
#include "core/resource/resource.h"
#include "core/resource/network_resource.h"
#include "core/resource/video_server.h"
#include "core/resource/qnstorage.h"
#include "core/misc/scheduleTask.h"
#include "core/resource/camera_resource.h"

class AppSessionManager;
class QnAppServerConnectionFactory;

namespace conn_detail
{
    class ReplyProcessor : public QObject
    {
        Q_OBJECT

    public:
        ReplyProcessor(QnResourceFactory& resourceFactory, const QString& objectName)
              : m_resourceFactory(resourceFactory),
                m_objectName(objectName)
        {
        }

    public slots:
        void at_replyReceived(int handle, int status, const QByteArray &reply);

    signals:
        void finished(int handle, int status, const QByteArray& errorString, const QnResourceList& resources);

    private:
        QnResourceFactory& m_resourceFactory;
        QString m_objectName;
    };
}

class QN_EXPORT QnAppServerConnection
{
public:
    ~QnAppServerConnection();

    void testConnectionAsync(QObject* target, const char* slot);

    bool isConnected() const;

    int getResourceTypes(QList<QnResourceTypePtr>& resourceTypes, QByteArray& errorString);

    int getResources(QList<QnResourcePtr>& resources, QByteArray& errorString);

    int addServer(const QnVideoServer&, QnVideoServerList& servers, QByteArray& errorString);
    int addCamera(const QnCameraResource&, QList<QnResourcePtr>& cameras, QByteArray& errorString);

    int addStorage(const QnStorage&, QByteArray& errorString);

    int getCameras(QnSecurityCamResourceList& cameras, const QnId& mediaServerId, QByteArray& errorString);
    int getStorages(QnResourceList& storages, QByteArray& errorString);

    // Returns request id
    int saveAsync(const QnVideoServer&, QObject*, const char*);
    int saveAsync(const QnCameraResource&, QObject*, const char*);

private:
    QnAppServerConnection(const QUrl &url, QnResourceFactory& resourceFactory);

private:
    QScopedPointer<AppSessionManager> m_sessionManager;
    QnResourceFactory& m_resourceFactory;
    QnVideoServerFactory m_serverFactory;

    friend class QnAppServerConnectionFactory;
};

typedef QSharedPointer<QnAppServerConnection> QnAppServerConnectionPtr;

class QN_EXPORT QnAppServerConnectionFactory
{
public:
    static QUrl defaultUrl();
    static void setDefaultUrl(const QUrl &url);

    static QnAppServerConnectionPtr createConnection(QnResourceFactory &resourceFactory);
    static QnAppServerConnectionPtr createConnection(const QUrl& url, QnResourceFactory &resourceFactory);

private:
    QMutex m_mutex;
    QUrl m_url;
};

#endif // APPSERVERCONNECTIONIMPL_H
