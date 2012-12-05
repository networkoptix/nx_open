#ifndef APPSERVERCONNECTIONIMPL_H
#define APPSERVERCONNECTIONIMPL_H

#include <QtCore/QMutex>
#include <QtCore/QUrl>

#include <QtNetwork/QNetworkReply>

#include "utils/common/request_param.h"

#include "core/resource/resource_type.h"
#include "core/resource/resource.h"
#include "core/resource/network_resource.h"
#include "core/resource/media_server_resource.h"
#include "core/resource/storage_resource.h"
#include "core/misc/schedule_task.h"
#include "core/resource/camera_resource.h"
#include "core/resource/layout_resource.h"
#include "core/misc/schedule_task.h"
#include "core/resource/user_resource.h"
#include "licensing/license.h"
#include "connectinfo.h"

#include "api/serializer/pb_serializer.h"

#include "api_fwd.h"

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
        void finishedLicense(int status, const QByteArray& errorString, QnLicenseList resources, int handle);
        void finishedConnect(int status, const QByteArray &errorString, QnConnectInfoPtr connectInfo, int requestHandle);

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
    int connectAsync(QObject* target, const char *slot);

    int connect(QnConnectInfoPtr& connectInfo, QByteArray &errorString);

    int getResourceTypes(QnResourceTypeList& resourceTypes, QByteArray& errorString);

    int getResources(QnResourceList& resources, QByteArray& errorString);
    int getResources(const QString& args, QnResourceList& resources, QByteArray& errorString);
    int getResource(const QnId& id, QnResourcePtr& resource, QByteArray& errorString);

    /**
      get resources synchronously

      @param args Currently we use args for passing id. Later we can introduce more sophisticated filters here.
    */
    int getResourcesAsync(const QString& args, const QString& objectName, QObject *target, const char *slot);
    int getLicensesAsync(QObject *target, const char *slot);

    int setResourceStatusAsync(const QnId& resourceId, QnResource::Status status , QObject *target, const char *slot);
    int setResourcesStatusAsync(const QnResourceList& resources, QObject *target, const char *slot);
    int setResourceStatus(const QnId& resourceId, QnResource::Status status, QByteArray& errorString);

    int setResourceDisabledAsync(const QnId& resourceId, bool disabled, QObject *target, const char *slot);
    int setResourcesDisabledAsync(const QnResourceList& resources, QObject *target, const char *slot);

    int registerServer(const QnMediaServerResourcePtr&, QnMediaServerResourceList& servers, QByteArray& authKey, QByteArray& errorString);
    int addCamera(const QnVirtualCameraResourcePtr&, QnVirtualCameraResourceList& cameras, QByteArray& errorString);

    int addStorage(const QnStorageResourcePtr&, QByteArray& errorString);
    int addCameraHistoryItem(const QnCameraHistoryItem& cameraHistoryItem, QByteArray& errorString);

    int getCameras(QnVirtualCameraResourceList& cameras, QnId mediaServerId, QByteArray& errorString);
    int getServers(QnMediaServerResourceList& servers, QByteArray& errorString);
    int getLayouts(QnLayoutResourceList& layouts, QByteArray& errorString);
    int getUsers(QnUserResourceList& users, QByteArray& errorString);
    int getLicenses(QnLicenseList& licenses, QByteArray& errorString);
    int getCameraHistoryList(QnCameraHistoryList& cameraHistoryList, QByteArray& errorString);

    int saveSync(const QnMediaServerResourcePtr&, QByteArray& errorString);
    int saveSync(const QnVirtualCameraResourcePtr&, QByteArray& errorString);

    // Returns request id
    int saveAsync(const QnMediaServerResourcePtr&, QObject*, const char*);
    int saveAsync(const QnVirtualCameraResourcePtr&, QObject*, const char*);
    int saveAsync(const QnUserResourcePtr&, QObject*, const char*);
    int saveAsync(const QnLayoutResourcePtr&, QObject*, const char*);

    int saveAsync(const QnLayoutResourceList&, QObject*, const char*);
    int saveAsync(const QnVirtualCameraResourceList& cameras, QObject* target, const char* slot);

    int saveAsync(const QnResourcePtr& resource, QObject* target, const char* slot);
    int addLicenseAsync(const QnLicensePtr& resource, QObject* target, const char* slot);

    int deleteAsync(const QnMediaServerResourcePtr&, QObject*, const char*);
    int deleteAsync(const QnVirtualCameraResourcePtr&, QObject*, const char*);
    int deleteAsync(const QnUserResourcePtr&, QObject*, const char*);
    int deleteAsync(const QnLayoutResourcePtr&, QObject*, const char*);

    int deleteAsync(const QnResourcePtr& resource, QObject* target, const char* slot);

    qint64 getCurrentTime();

    void stop();

    static int getMediaProxyPort();

private:
    QnAppServerConnection(const QUrl &url, QnResourceFactory& resourceFactory, QnApiSerializer& serializer, const QString& guid, const QString& authKey);

    int connectAsync_i(const QnRequestHeaderList& headers, const QnRequestParamList& params, QObject* target, const char *slot);

    int getObjectsAsync(const QString& objectName, const QString& args, QObject* target, const char* slot);
    int getObjects(const QString& objectName, const QString& args, QByteArray& data, QByteArray& errorString);

    int addObjectAsync(const QString& objectName, const QByteArray& data, QObject* target, const char* slot);
    int addObject(const QString& objectName, const QByteArray& body, QnReplyHeaderList& replyHeaders, QByteArray& response, QByteArray& errorString);

    int deleteObjectAsync(const QString& objectName, int id, QObject* target, const char* slot);


private:
    QUrl m_url;
    QnRequestParamList m_requestParams;
    QnRequestHeaderList m_requestHeaders;

    QnResourceFactory& m_resourceFactory;
    QnMediaServerResourceFactory m_serverFactory;

    QnApiSerializer& m_serializer;

    friend class QnAppServerConnectionFactory;
};

class QN_EXPORT QnAppServerConnectionFactory
{
public:
    static QString authKey();
    static QString clientGuid();
    static QUrl defaultUrl();
    static int defaultMediaProxyPort();
    static QString currentVersion();
	static QnResourceFactory* defaultFactory();

    static void setAuthKey(const QString &key);
    static void setClientGuid(const QString &guid);
    static void setDefaultUrl(const QUrl &url);
    static void setDefaultFactory(QnResourceFactory*);
    static void setDefaultMediaProxyPort(int port);
    static void setCurrentVersion(const QString& version);

    static QnAppServerConnectionPtr createConnection();
    static QnAppServerConnectionPtr createConnection(const QUrl& url);

private:
    QMutex m_mutex;
    QString m_clientGuid;
    QString m_authKey;
    QUrl m_defaultUrl;
    int m_defaultMediaProxyPort;
    QString m_currentVersion;
    QnResourceFactory* m_resourceFactory;
    QnApiPbSerializer m_serializer;
};

bool initResourceTypes(QnAppServerConnectionPtr appServerConnection);
bool initLicenses(QnAppServerConnectionPtr appServerConnection);
bool initCameraHistory(QnAppServerConnectionPtr appServerConnection);

#endif // APPSERVERCONNECTIONIMPL_H
