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
#include "core/resource/layout_data.h"
#include "core/misc/scheduleTask.h"

#include "api/serializer/xml_serializer.h"

class AppSessionManager;
class QnAppServerConnectionFactory;

class QnApiSerializer;

namespace conn_detail
{
    class ReplyProcessor : public QObject
    {
        Q_OBJECT

    public:
        ReplyProcessor(QnResourceFactory& resourceFactory, QnApiSerializer& serializer, const QString& objectName)
              : m_resourceFactory(resourceFactory),
                m_serializer(serializer),
                m_objectName(objectName)
        {
        }

    public slots:
        void finished(int status, const QByteArray &result, int handle);

    signals:
        void finished(int handle, int status, const QByteArray& errorString, QnResourceList cameras);

    private:
        QnResourceFactory& m_resourceFactory;
        QnApiSerializer& m_serializer;
        QString m_objectName;
    };
}

class QN_EXPORT QnAppServerConnection
{
public:
    ~QnAppServerConnection();

    void testConnectionAsync(QObject* target, const char* slot);

    bool isConnected() const;

    int getResourceTypes(QnResourceTypeList& resourceTypes, QByteArray& errorString);

    int getResources(QList<QnResourcePtr>& resources, QByteArray& errorString);

    int registerServer(const QnVideoServerPtr&, QnVideoServerList& servers, QByteArray& errorString);
    int addCamera(const QnCameraResourcePtr&, QnCameraResourceList& cameras, QByteArray& errorString);

    int addStorage(const QnStoragePtr&, QByteArray& errorString);

    int getCameras(QnCameraResourceList& cameras, QnId mediaServerId, QByteArray& errorString);
    int getStorages(QnStorageList& storages, QByteArray& errorString);
    int getLayouts(QnLayoutDataList& layouts, QByteArray& errorString);

    // Returns request id
    int saveAsync(const QnVideoServerPtr&, QObject*, const char*);
    int saveAsync(const QnCameraResourcePtr&, QObject*, const char*);

    int saveAsync(const QnResourcePtr& resource, QObject* target, const char* slot);

private:
    QnAppServerConnection(const QUrl &url, QnResourceFactory& resourceFactory);

private:
    QScopedPointer<AppSessionManager> m_sessionManager;
    QnResourceFactory& m_resourceFactory;
    QnVideoServerFactory m_serverFactory;

    QnApiXmlSerializer m_serializer;

    friend class QnAppServerConnectionFactory;
};

typedef QSharedPointer<QnAppServerConnection> QnAppServerConnectionPtr;

class QN_EXPORT QnAppServerConnectionFactory
{
public:
    static QUrl defaultUrl();
    static void setDefaultUrl(const QUrl &url);
    static void setDefaultFactory(QnResourceFactory*);

    static QnAppServerConnectionPtr createConnection();
    static QnAppServerConnectionPtr createConnection(const QUrl& url);

private:
    QMutex m_mutex;
    QUrl m_url;
    QnResourceFactory* m_resourceFactory;
};

bool initResourceTypes(QnAppServerConnectionPtr appServerConnection);

#endif // APPSERVERCONNECTIONIMPL_H
