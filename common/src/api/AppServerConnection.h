#ifndef APPSERVERCONNECTIONIMPL_H
#define APPSERVERCONNECTIONIMPL_H

#include <QtCore/QMutex>
#include <QtCore/QUrl>

#include "core/resource/resource_type.h"
#include "core/resource/resource.h"
#include "core/resource/network_resource.h"
#include "core/resource/video_server.h"
#include "core/resource/storage_resource.h"
#include "core/misc/scheduleTask.h"
#include "core/resource/camera_resource.h"
#include "core/resource/layout_resource.h"
#include "core/misc/scheduleTask.h"
#include "core/resource/user_resource.h"
#include "licensing/license.h"

#include "api/serializer/pb_serializer.h"

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
        void finished(int status, const QByteArray &result, const QByteArray &errorString, int handle);

    signals:
        void finished(int status, const QByteArray& errorString, QnResourceList resources, int handle);

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

    int testConnectionAsync(QObject* target, const char* slot);

    int getResourceTypes(QnResourceTypeList& resourceTypes, QByteArray& errorString);

    int getResources(QList<QnResourcePtr>& resources, QByteArray& errorString);

	/**
	  get resources synchronously

	  @param args Currently we use args for passing id. Later we can introduce more sophisticated filters here.
	*/
	int getResourcesAsync(const QString& args, const QString& objectName, QObject *target, const char *slot);
	int setResourceStatusAsync(const QnId& resourceId, QnResource::Status status , QObject *target, const char *slot);

    int registerServer(const QnVideoServerResourcePtr&, QnVideoServerResourceList& servers, QByteArray& errorString);
    int addCamera(const QnVirtualCameraResourcePtr&, QnVirtualCameraResourceList& cameras, QByteArray& errorString);

    int addStorage(const QnStorageResourcePtr&, QByteArray& errorString);

    int getCameras(QnVirtualCameraResourceList& cameras, QnId mediaServerId, QByteArray& errorString);
    int getServers(QnVideoServerResourceList& servers, QByteArray& errorString);
    int getLayouts(QnLayoutResourceList& layouts, QByteArray& errorString);
    int getUsers(QnUserResourceList& users, QByteArray& errorString);
    int getLicenses(QnLicenseList& licenses, QByteArray& errorString);

    int saveSync(const QnVideoServerResourcePtr&, QByteArray& errorString);
    int saveSync(const QnVirtualCameraResourcePtr&, QByteArray& errorString);

    // Returns request id
    int saveAsync(const QnVideoServerResourcePtr&, QObject*, const char*);
    int saveAsync(const QnVirtualCameraResourcePtr&, QObject*, const char*);
    int saveAsync(const QnUserResourcePtr&, QObject*, const char*);
    int saveAsync(const QnLayoutResourcePtr&, QObject*, const char*);

    int saveAsync(const QnLayoutResourceList&, QObject*, const char*);
    int saveAsync(const QnVirtualCameraResourceList& cameras, QObject* target, const char* slot);

    int saveAsync(const QnResourcePtr& resource, QObject* target, const char* slot);

    int deleteAsync(const QnVideoServerResourcePtr&, QObject*, const char*);
    int deleteAsync(const QnVirtualCameraResourcePtr&, QObject*, const char*);
    int deleteAsync(const QnUserResourcePtr&, QObject*, const char*);
    int deleteAsync(const QnLayoutResourcePtr&, QObject*, const char*);

    int deleteAsync(const QnResourcePtr& resource, QObject* target, const char* slot);

    qint64 getCurrentTime();

    void stop();

private:
    QnAppServerConnection(const QUrl &url, QnResourceFactory& resourceFactory);

	int getObjectsAsync(const QString& objectName, const QString& args, QObject* target, const char* slot);
	int getObjects(const QString& objectName, const QString& args, QByteArray& data, QByteArray& errorString);

	int addObjectAsync(const QString& objectName, const QByteArray& data, QObject* target, const char* slot);
	int addObject(const QString& objectName, const QByteArray& body, QByteArray& response, QByteArray& errorString);

    int deleteObjectAsync(const QString& objectName, int id, QObject* target, const char* slot);


private:
    QUrl m_url;
    QnRequestParamList m_requestParams;

    QnResourceFactory& m_resourceFactory;
    QnVideoServerResourceFactory m_serverFactory;

    QnApiPbSerializer m_serializer;

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
    QUrl m_defaultUrl;
    QnResourceFactory* m_resourceFactory;
};

bool initResourceTypes(QnAppServerConnectionPtr appServerConnection);
bool initLicenses(QnAppServerConnectionPtr appServerConnection);

#endif // APPSERVERCONNECTIONIMPL_H
