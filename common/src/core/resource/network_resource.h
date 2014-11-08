#ifndef QN_NETWORK_RESOURCE_H
#define QN_NETWORK_RESOURCE_H

#include <QtNetwork/QAuthenticator>
#include <QtNetwork/QHostAddress>
#include "utils/network/mac_address.h"
#include "resource.h"

class QnTimePeriodList;

class QN_EXPORT QnNetworkResource : public QnResource
{
    Q_OBJECT
    //Q_PROPERTY(QHostAddress hostAddress READ getHostAddress WRITE setHostAddress)
    //Q_PROPERTY(QnMacAddress macAddress READ getMAC WRITE setMAC)
    //Q_PROPERTY(QAuthenticator auth READ getAuth WRITE setAuth)

public:
    enum NetworkStatusFlag {
        BadHostAddr = 0x01,
        Ready = 0x04
    };
    Q_DECLARE_FLAGS(NetworkStatus, NetworkStatusFlag)

    QnNetworkResource();
    virtual ~QnNetworkResource();

    virtual QString getUniqueId() const;

    virtual QString getHostAddress() const;
    virtual void setHostAddress(const QString &ip);

    QnMacAddress getMAC() const;
    void setMAC(const QnMacAddress &mac);

    QString getPhysicalId() const;
    void setPhysicalId(const QString& physicalId);

    void setAuth(const QString &user, const QString &password);
    void setAuth(const QAuthenticator &auth);

    void setDefaultAuth(const QString &user, const QString &password);
    void setDefaultAuth(const QAuthenticator &auth);

    QAuthenticator getAuth() const;

    // if reader will find out that authentication is requred => setAuthenticated(false) must be called
    bool isAuthenticated() const;
    void setAuthenticated(bool auth);

    virtual int httpPort() const;
    virtual void setHttpPort( int newPort );

    /*!
        By default, it is rtsp port (554)
    */
    virtual int mediaPort() const;
    void setMediaPort( int newPort );

    virtual QString toString() const;
    QString toSearchString() const;


    void addNetworkStatus(NetworkStatus status);
    void removeNetworkStatus(NetworkStatus status);
    bool checkNetworkStatus(NetworkStatus status) const;
    void setNetworkStatus(NetworkStatus status);


    // all data readers and any sockets will use this number as timeout value in ms
    void setNetworkTimeout(unsigned int timeout);
    virtual unsigned int getNetworkTimeout() const;

    virtual void updateInner(const QnResourcePtr &other, QSet<QByteArray>& modifiedFields) override;

    // in some cases I just want to update couple of field from just discovered resource
    virtual bool mergeResourcesIfNeeded(const QnNetworkResourcePtr &source);

    virtual int getChannel() const;

    /*
    * Return time periods from resource based archive (direct to storage)
    */
    virtual QnTimePeriodList getDtsTimePeriods(qint64 startTimeMs, qint64 endTimeMs, int detailLevel);

    //!Returns true if camera is accessible
    /*!
        Default implementation just establishes connection to \a getHostAddress() : \a httpPort()
        \todo #ak This method is used in diagnostics only. Throw it away and use \a QnNetworkResource::checkIfOnline instead
    */
    virtual bool ping();
    //!Checks if camera is online
    /*!
        \note Implementation MUST check not only camera address:port accessibility, but also check some unique parameters of camera
        \note Default implementation returns false
    */
    virtual bool checkIfOnline();

    static QnUuid uniqueIdToId(const QString& uniqId);
    virtual bool isAbstractResource() const { return false; }
    virtual void initializationDone() override;
private:
    //QAuthenticator m_auth;
    bool m_authenticated;

    //QHostAddress m_hostAddr;
    QnMacAddress m_macAddress;
    QString m_physicalId;

    NetworkStatus m_networkStatus;

    unsigned int m_networkTimeout;
    int m_httpPort;
    int m_mediaPort;

    bool m_probablyNeedToUpdateStatus;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QnNetworkResource::NetworkStatus)

#endif // QN_NETWORK_RESOURCE_H
