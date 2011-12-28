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
#include "core/resourcemanagment/security_cam_resource.h"

class AppSessionManager;
class QnAppServerConnectionFactory;

class QN_EXPORT QnAppServerConnection
{
public:
    ~QnAppServerConnection();

    void testConnectionAsync(QObject* target, const char* slot);

    bool isConnected() const;

    int getResourceTypes(QList<QnResourceTypePtr>& resourceTypes);

    int getResources(QList<QnResourcePtr>& resources);

    int addServer(const QnVideoServer&, QnVideoServerList& servers);
    int addCamera(const QnNetworkResource&, const QnId& serverId, QList<QnResourcePtr>& cameras);
    int addStorage(const QnStorage&);

    int getCameras(QnSequrityCamResourceList& cameras, const QnId& mediaServerId);
    int getStorages(QnResourceList& storages);
    int getScheduleTasks(QnScheduleTaskList& scheduleTasks, const QnId& mediaServerId);

    QString lastError() const;

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
    static QnAppServerConnectionPtr createConnection(QUrl url, QnResourceFactory &resourceFactory);

private:
    QMutex m_mutex;
    QUrl m_url;
};

#endif // APPSERVERCONNECTIONIMPL_H
