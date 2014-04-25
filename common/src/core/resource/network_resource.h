#ifndef QN_NETWORK_RESOURCE_H
#define QN_NETWORK_RESOURCE_H

#include <QtNetwork/QAuthenticator>
#include <QtNetwork/QHostAddress>
#include "utils/network/mac_address.h"
#include "resource.h"
#include "recording/time_period_list.h"

class QN_EXPORT QnNetworkResource : public QnResource
{
    Q_OBJECT
    //Q_PROPERTY(QHostAddress hostAddress READ getHostAddress WRITE setHostAddress)
    //Q_PROPERTY(QnMacAddress macAddress READ getMAC WRITE setMAC)
    //Q_PROPERTY(QAuthenticator auth READ getAuth WRITE setAuth)

public:
    enum QnNetworkStatus
    {
        BadHostAddr = 0x01,
        Ready = 0x04
    };

    QnNetworkResource();
    virtual ~QnNetworkResource();

    virtual QString getUniqueId() const;

    virtual QString getHostAddress() const;
    virtual bool setHostAddress(const QString &ip, QnDomain domain = QnDomainMemory);

    QnMacAddress getMAC() const;
    void setMAC(const QnMacAddress &mac);

    QString getPhysicalId() const;
    void setPhysicalId(const QString& physicalId);

    inline void setAuth(const QString &user, const QString &password)
    { QAuthenticator auth; auth.setUser(user); auth.setPassword(password); setAuth(auth); }
    void setAuth(const QAuthenticator &auth);
    QAuthenticator getAuth() const;

    // if reader will find out that authentication is requred => setAuthenticated(false) must be called
    bool isAuthenticated() const;
    void setAuthenticated(bool auth);

    // address used to discover this resource ( in case if machine has more than one NIC/address)
    // by default we assume that password is login and password are known and getDiscoveryAddr returns true
    QHostAddress getDiscoveryAddr() const;
    void setDiscoveryAddr(QHostAddress addr);

    virtual int httpPort() const;

    virtual QString toString() const;
    QString toSearchString() const;


    void addNetworkStatus(QnNetworkStatus status);
    void removeNetworkStatus(QnNetworkStatus status);
    bool checkNetworkStatus(QnNetworkStatus status) const;
    void setNetworkStatus(QnNetworkStatus status);


    // all data readers and any sockets will use this number as timeout value in ms
    void setNetworkTimeout(unsigned int timeout);
    virtual unsigned int getNetworkTimeout() const;


    // sometimes resource is not in your lan, and it might be not pingable from one hand
    // but from other hand it's still might replay to standard requests
    // so this is the way to find out do we have to change ip address
    virtual bool isResourceAccessible()  = 0;


    // is some cases( like  device behind the router) the only possible way to discover the device is to check every ip address
    // and no broad cast and multi cast is accessible. so you can not get MAC of device with standard methods
    // the only way is to request it from device through http or so
    // we need to get mac anyway to differentiate one device from another
    virtual bool updateMACAddress() { return true; }

    virtual void updateInner(const QnResourcePtr &other, QSet<QByteArray>& modifiedFields) override;

    // in some cases I just want to update couple of field from just discovered resource
    virtual bool mergeResourcesIfNeeded(const QnNetworkResourcePtr &source);

    virtual int getChannel() const;

    /*
    * Return time periods from resource based archive (direct to storage)
    */
    virtual QnTimePeriodList getDtsTimePeriods(qint64 startTimeMs, qint64 endTimeMs, int detailLevel) {
        Q_UNUSED(startTimeMs)
        Q_UNUSED(endTimeMs)
        Q_UNUSED(detailLevel)
        return QnTimePeriodList(); }

    //!Returns true if camera is accessible
    /*!
        Default implementation just establishes connection to \a getHostAddress() : \a httpPort()
    */
    virtual bool ping();

private:
    QAuthenticator m_auth;
    bool m_authenticated;

    //QHostAddress m_hostAddr;
    QnMacAddress m_macAddress;
    QString m_physicalId;

    QHostAddress m_localAddress; // address used to discover this resource ( in case if machine has more than one NIC/address

    unsigned long m_networkStatus;

    unsigned int m_networkTimeout;

    bool m_probablyNeedToUpdateStatus;
};

#endif // QN_NETWORK_RESOURCE_H
