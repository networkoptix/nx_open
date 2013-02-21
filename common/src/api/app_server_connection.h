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
        void finished(const QnHTTPRawResponse& response, int handle);

    signals:
        void finished(int status, const QByteArray& errorString, QnResourceList resources, int handle);
        void finishedLicense(int status, const QByteArray &errorString, QnLicenseList licenses, int handle);
        void finishedKvPair(int status, const QByteArray& errorString, QnKvPairList kvPairs, int handle);
        void finishedConnect(int status, const QByteArray &errorString, QnConnectInfoPtr connectInfo, int handle);
        void finishedBusinessRule(int status, const QByteArray &errorString, QnBusinessEventRules businessRules, int handle);
        void finishedSetting(int status, const QByteArray& errorString, QnKvPairList settings, int handle);
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

    void stop();
    static int getMediaProxyPort();
    const QByteArray& getLastError() const;

    // Synchronous API
    int connect(QnConnectInfoPtr& connectInfo);
    int getResourceTypes(QnResourceTypeList& resourceTypes);
    int getResources(QnResourceList& resources);
    int getResources(const QString& args, QnResourceList& resources);
    int getResource(const QnId& id, QnResourcePtr& resource);
    int getBusinessRules(QnBusinessEventRules &businessRules);

    int setResourceStatus(const QnId& resourceId, QnResource::Status status);
    int saveServer(const QnMediaServerResourcePtr&, QnMediaServerResourceList& servers, QByteArray& authKey);
    int addCamera(const QnVirtualCameraResourcePtr&, QnVirtualCameraResourceList& cameras);
    int addCameraHistoryItem(const QnCameraHistoryItem& cameraHistoryItem);
    int addBusinessRule(const QnBusinessEventRulePtr &businessRule);
    bool setPanicMode(QnMediaServerResource::PanicMode value);

    int getCameras(QnVirtualCameraResourceList& cameras, QnId mediaServerId);
    int getServers(QnMediaServerResourceList& servers);
    int getLayouts(QnLayoutResourceList& layouts);
    int getUsers(QnUserResourceList& users);
    int getLicenses(QnLicenseList& licenses);
    int getCameraHistoryList(QnCameraHistoryList& cameraHistoryList);

    int saveSync(const QnMediaServerResourcePtr&);
    int saveSync(const QnVirtualCameraResourcePtr&);

    int sendEmail(const QString& to, const QString& subject, const QString& message);
    int sendEmail(const QStringList& to, const QString& subject, const QString& message);
    qint64 getCurrentTime();

    // Asynchronous API
    int testConnectionAsync(QObject* target, const char* slot);
    int connectAsync(QObject* target, const char *slot);
    int getResourcesAsync(const QString& args, const QString& objectName, QObject *target, const char *slot);
    int getLicensesAsync(QObject *target, const char *slot);
    int getBusinessRulesAsync(QObject *target, const char *slot);
    int getKvPairsAsync(QObject* target, const char* slot);
    int getSettingsAsync(QObject *target, const char *slot);

    int setResourceStatusAsync(const QnId& resourceId, QnResource::Status status , QObject *target, const char *slot);
    int setResourcesStatusAsync(const QnResourceList& resources, QObject *target, const char *slot);

    int setResourceDisabledAsync(const QnId& resourceId, bool disabled, QObject *target, const char *slot);
    int setResourcesDisabledAsync(const QnResourceList& resources, QObject *target, const char *slot);

    int dumpDatabase(QObject *target, const char *slot);
    int restoreDatabase(const QByteArray& data, QObject *target, const char *slot);

    // Returns request id
    int saveAsync(const QnMediaServerResourcePtr&, QObject*, const char*);
    int saveAsync(const QnVirtualCameraResourcePtr&, QObject*, const char*);
    int saveAsync(const QnUserResourcePtr&, QObject*, const char*);
    int saveAsync(const QnLayoutResourcePtr&, QObject*, const char*);
    int saveAsync(const QnBusinessEventRulePtr& rule, QObject* target, const char* slot);

    int saveAsync(const QnLayoutResourceList&, QObject*, const char*);
    int saveAsync(const QnVirtualCameraResourceList& cameras, QObject* target, const char* slot);

    int saveAsync(const QnResourcePtr& resource, QObject* target, const char* slot);
    int addLicenseAsync(const QnLicensePtr& resource, QObject* target, const char* slot);

    int saveAsync(const QnKvPairList& kvPairs, QObject* target, const char* slot);
    int saveSettingsAsync(const QnKvPairList& kvPairs/*, QObject* target, const char* slot*/);

    int deleteAsync(const QnMediaServerResourcePtr&, QObject*, const char*);
    int deleteAsync(const QnVirtualCameraResourcePtr&, QObject*, const char*);
    int deleteAsync(const QnUserResourcePtr&, QObject*, const char*);
    int deleteAsync(const QnLayoutResourcePtr&, QObject*, const char*);

    int deleteRuleAsync(int ruleId, QObject* target, const char* slot);

    int deleteAsync(const QnResourcePtr& resource, QObject* target, const char* slot);

    bool broadcastBusinessAction(const QnAbstractBusinessActionPtr& businessAction);
private:
    QnAppServerConnection(const QUrl &url, QnResourceFactory& resourceFactory, QnApiSerializer& serializer, const QString& guid, const QString& authKey);

    int connectAsync_i(const QnRequestHeaderList& headers, const QnRequestParamList& params, QObject* target, const char *slot);

    int getObjectsAsync(const QString& objectName, const QString& args, QObject* target, const char* slot);
    int getObjects(const QString& objectName, const QString& args, QnHTTPRawResponse& response);

    int addObjectAsync(const QString& objectName, const QByteArray& data, QObject* target, const char* slot);
    int addObject(const QString& objectName, const QByteArray& body, QnHTTPRawResponse &response);

    int deleteObjectAsync(const QString& objectName, int id, QObject* target, const char* slot);

private:
    // By now this is used only by synchronous api.
    // TODO: Make use for asynch API as well
    // TODO: #Ivan 
    QByteArray m_lastError;

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
