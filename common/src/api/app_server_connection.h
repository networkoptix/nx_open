#ifndef QN_APP_SERVER_CONNECTION_H
#define QN_APP_SERVER_CONNECTION_H

#include <QtCore/QMutex>

#include <utils/common/request_param.h>

#include <core/resource/resource_fwd.h>
#include <core/misc/schedule_task.h>

#include <licensing/license.h>
#include <nx_ec/ec_api.h>

#include <api/model/servers_reply.h>
#include <api/model/connection_info.h>

#include "api_fwd.h"
#include "abstract_connection.h"

class QnAppServerConnectionFactory;
class QnApiSerializer;

class QnAppServerReplyProcessor: public QnAbstractReplyProcessor {
    Q_OBJECT

public:
    QnAppServerReplyProcessor(QnResourceFactory &resourceFactory, QnApiSerializer &serializer, int object);
    virtual ~QnAppServerReplyProcessor();

    QString errorString() const { return m_errorString; }

public slots:
    void processReply(const QnHTTPRawResponse &response, int handle);

signals:
    void finished(int status, const QnCameraHistoryList &reply, int handle);
    void finished(int status, const QnResourceList &reply, int handle);
    void finished(int status, const QnResourceTypeList &reply, int handle);
    void finished(int status, const QnServersReply &reply, int handle);
    void finished(int status, const QnLicenseList &reply, int handle);
    void finished(int status, const QnKvPairListsById &reply, int handle);
    void finished(int status, const QnConnectionInfoPtr &reply, int handle);
    void finished(int status, const QnBusinessEventRuleList &reply, int handle);
    void finished(int status, const QnKvPairList &reply, int handle);
    void finished(int status, bool reply, int handle);
    void finished(int status, const QByteArray &reply, int handle);
    void finished(int status, const QStringList &reply, int handle);

private:
    friend class QnAbstractRepqlyProcessor;

    QnResourceFactory &m_resourceFactory;
    QnApiSerializer &m_serializer;
    QString m_errorString;
};


class QN_EXPORT QnAppServerConnection: public QnAbstractConnection {
public:
    virtual ~QnAppServerConnection();

    void stop();
    static int getMediaProxyPort();
    QByteArray getLastError() const;

    // Synchronous API
    int connect(QnConnectionInfoPtr& connectInfo);
    int getResourceTypes(QnResourceTypeList& resourceTypes);
    //int getResources(QnResourceList& resources);
    int getResource(const QnId& id, QnResourcePtr& resource);
    int getBusinessRules(QnBusinessEventRuleList &businessRules);

    int setResourceStatus(const QnId& resourceId, QnResource::Status status);
    int saveServer(const QnMediaServerResourcePtr&, QnMediaServerResourceList& servers, QByteArray& authKey);
    int addCamera(const QnVirtualCameraResourcePtr&, QnVirtualCameraResourceList& cameras);
    int addCameraHistoryItem(const QnCameraHistoryItem& cameraHistoryItem);
    int addBusinessRule(const QnBusinessEventRulePtr &businessRule);
    bool setPanicMode(int value);
    void disconnectSync();

    int getKvPairs(QnKvPairList& kvPairs, const QnResourcePtr &resource);
    int getCameras(QnVirtualCameraResourceList& cameras, QnId mediaServerId);
    int getServers(QnMediaServerResourceList& servers);
    //int getLayouts(QnLayoutResourceList& layouts);
    //int getUsers(QnUserResourceList& users);
    int getLicenses(QnLicenseList& licenses);
    int getCameraHistoryList(QnCameraHistoryList& cameraHistoryList);
    qint64 getCurrentTime(); // TODO: #Elric this method doesn't follow the sync api guidelines

    int saveSync(int resourceId, const QnKvPair &kvPair);
    int saveSync(int resourceId, const QnKvPairList &kvPairs);
    int saveSync(const QnMediaServerResourcePtr &resource);
    int saveSync(const QnVirtualCameraResourcePtr &resource);


    /**
      * Test if email settings are valid
      * 
      * Slot is (int status, const QByteArray& errorString, bool result, int handle),
      * where result is true if settings are valid
      * 
      * @return connection handle
      */
    int testEmailSettingsAsync(const QnKvPairList &settings, QObject *target, const char *slot);

    int sendEmailAsync(const QString& to, const QString& subject, const QString& message, int timeout, QObject *target, const char *slot);
    int sendEmailAsync(const QStringList& to, const QString& subject, const QString& message, int timeout, QObject *target, const char *slot);
    int sendEmailAsync(const QStringList &to, const QString& subject, const QString& message, const QnEmailAttachmentList& attachments, int timeout, QObject *target, const char *slot);

    /**
     * @brief requestStoredFileAsync        Get stored file from EC
     * @param filename                      Name of the file
     * @param target                        Receiver object
     * @param slot                          SLOT(int status, const QByteArray& data, int handle)
     * @return                              Handle of the request
     */
    //int requestStoredFileAsync(const QString &filename, QObject *target, const char *slot);

    /**
     * @brief addStoredFileAsync            Put new file to EC
     * @param filename                      Name of the file. Should contain GUID to avoid collisions
     * @param data                          Contents of the file
     * @param target                        Receiver object
     * @param slot                          SLOT(int status, int handle)
     * @return                              Handle of the request
     */
    //int addStoredFileAsync(const QString &filename, const QByteArray &data, QObject *target, const char *slot);

    /**
     * @brief deleteStoredFileAsync         Delete stored file from EC
     * @param filename                      Name of the file
     * @param target                        Receiver object
     * @param slot                          SLOT(int status, int handle)
     * @return                              Handle of the request
     */
    //int deleteStoredFileAsync(const QString &filename, QObject *target, const char *slot);

    /**
     * @brief requestDirectoryListingAsync  Get filenames for all stored files on EC in the selected directory
     * @param folderName                    Name of the directory
     * @param target                        Receiver object
     * @param slot                          SLOT(int status, const QStringList& filenames, int handle)
     * @return                              Handle of the request
     */
    //int requestDirectoryListingAsync(const QString &folderName, QObject *target, const char *slot);

    //int testConnectionAsync(QObject *target, const char *slot);
    int connectAsync(QObject *target, const char *slot);
    //int getLicensesAsync(QObject *target, const char *slot);
    //int getBusinessRulesAsync(QObject *target, const char *slot);
    //int getKvPairsAsync(const QnResourcePtr &resource, QObject *target, const char *slot);
    //int getAllKvPairsAsync(QObject *target, const char *slot);
    //int getSettingsAsync(QObject *target, const char *slot);

    //int setResourceStatusAsync(const QnId &resourceId, QnResource::Status status, QObject *target, const char *slot);
    //int setResourcesStatusAsync(const QnResourceList& resources, QObject *target, const char *slot);

    //int dumpDatabaseAsync(QObject *target, const char *slot);
    //int restoreDatabaseAsync(const QByteArray &data, QObject *target, const char *slot);

    //int saveAsync(const QnMediaServerResourcePtr &resource, QObject *target, const char *slot);
    //int saveAsync(const QnVirtualCameraResourcePtr &resource, QObject *target, const char *slot);
    //int saveAsync(const QnUserResourcePtr &resource, QObject *target, const char *slot);
    //int saveAsync(const QnLayoutResourcePtr &resource, QObject *target, const char *slot);
    //int saveAsync(const QnBusinessEventRulePtr &rule, QObject *target, const char *slot);

    //int saveAsync(const QnLayoutResourceList &resources, QObject *target, const char *slot);
    //int saveAsync(const QnVirtualCameraResourceList &cameras, QObject *target, const char *slot);

    //int saveAsync(const QnResourcePtr &resource, QObject *target, const char *slot);
    //int addLicensesAsync(const QList<QnLicensePtr> &licenses, QObject *target, const char *slot);

    //int saveAsync(int resourceId, const QnKvPairList &kvPairs, QObject *target = NULL, const char *slot = NULL);
    //int saveSettingsAsync(const QnKvPairList& kvPairs, QObject* target, const char* slot);

    //int deleteAsync(const QnMediaServerResourcePtr &resource, QObject *target, const char *slot);
    //int deleteAsync(const QnVirtualCameraResourcePtr &resource, QObject *target, const char *slot);
    //int deleteAsync(const QnUserResourcePtr &resource, QObject *target, const char *slot);
    //int deleteAsync(const QnLayoutResourcePtr &resource, QObject *target, const char *slot);

    //int deleteRuleAsync(int ruleId, QObject *target, const char *slot);

    //int deleteAsync(const QnResourcePtr &resource, QObject *target, const char *slot);

    //int broadcastBusinessAction(const QnAbstractBusinessActionPtr &businessAction, QObject *target, const char *slot);

    int resetBusinessRulesAsync(QObject *target, const char *slot);

protected:
    virtual QnAbstractReplyProcessor *newReplyProcessor(int object) override;

private:
    QnAppServerConnection(const QUrl &url, QnResourceFactory &resourceFactory, QnApiSerializer &serializer, const QString &guid, const QString &authKey);

    int connectAsync_i(const QnRequestHeaderList &headers, const QnRequestParamList &params, QObject *target, const char *slot);

    int getObjects(int object, const QString &args, QnHTTPRawResponse &response);

    int addObjectAsync(int object, const QByteArray &data, const char *replyTypeName, QObject *target, const char *slot);
    int addObject(int object, const QByteArray &body, QVariant *reply);

    int deleteObjectAsync(int object, int id, QObject *target, const char *slot);

private:
    friend class QnAppServerConnectionFactory;

    QnRequestParamList m_requestParams;
    QnRequestHeaderList m_requestHeaders;

    QnResourceFactory &m_resourceFactory;

    QnApiSerializer& m_serializer;
};


class QN_EXPORT QnAppServerConnectionFactory 
{
public:
    QnAppServerConnectionFactory();
    virtual ~QnAppServerConnectionFactory();

    static QString authKey();
    static QString clientGuid();
    static QUrl defaultUrl();
    static QUrl publicUrl();
    static QByteArray prevSessionKey();
    static QByteArray sessionKey();
    static QString systemName();
    static int defaultMediaProxyPort();
    static QnSoftwareVersion currentVersion();
    static QnResourceFactory* defaultFactory();

    static void setAuthKey(const QString &key);
    static void setClientGuid(const QString &guid);
    static void setDefaultUrl(const QUrl &url);
    static void setDefaultFactory(QnResourceFactory *);
    static void setDefaultMediaProxyPort(int port);
    static void setCurrentVersion(const QnSoftwareVersion &version);
    static void setPublicIp(const QString &publicIp);
    static void setSystemName(const QString& systemName);
    static void setAllowCameraCHanges(bool value);
    static bool isAllowCameraCHanges();

    static void setSessionKey(const QByteArray& sessionKey);

    //static QnAppServerConnectionPtr createConnection();
    //static QnAppServerConnectionPtr createConnection(const QUrl &url);

    static void setEC2ConnectionFactory( ec2::AbstractECConnectionFactory* ec2ConnectionFactory );
    static ec2::AbstractECConnectionFactory* ec2ConnectionFactory();
    static void setEc2Connection( ec2::AbstractECConnectionPtr connection );
    static ec2::AbstractECConnectionPtr getConnection2();

private:
    QMutex m_mutex;
    QString m_clientGuid;
    QString m_authKey;
    QUrl m_defaultUrl;
    QUrl m_publicUrl;
    QString m_systemName;
    QByteArray m_sessionKey;
    QByteArray m_prevSessionKey;

    int m_defaultMediaProxyPort;
    QnSoftwareVersion m_currentVersion;
    QnResourceFactory *m_resourceFactory;
    QScopedPointer<QnApiSerializer> m_serializer;
    bool m_allowCameraChanges;
};

bool initResourceTypes(QnAppServerConnectionPtr appServerConnection);
bool initResourceTypes(ec2::AbstractECConnectionPtr ec2Connection);
bool initLicenses(QnAppServerConnectionPtr appServerConnection);
bool initCameraHistory(QnAppServerConnectionPtr appServerConnection);

#endif // QN_APP_SERVER_CONNECTION_H
