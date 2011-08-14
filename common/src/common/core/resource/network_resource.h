#ifndef network_device_h_1249
#define network_device_h_1249

#include <QtNetwork/QAuthenticator>
#include <QtNetwork/QHostAddress>

#include "resource.h"
#include "network/mac_address.h"

class QnNetworkResource;
typedef QSharedPointer<QnNetworkResource> QnNetworkResourcePtr;

class QnNetworkResource : virtual public QnResource
{
public:
    enum
    {
        NotSameLAN = 0x01,
        HasConflicts = 0x02
    };

    QnNetworkResource();
    virtual ~QnNetworkResource();

    virtual bool equalsTo(const QnResourcePtr other) const;

    QHostAddress getHostAddress() const;
    virtual bool setHostAddress(const QHostAddress& ip, QnDomain domain );

	QnMacAddress getMAC() const;
	void  setMAC(QnMacAddress mac);

	void setAuth(const QString& user, QString password);
	QAuthenticator getAuth() const;

	// address used to discover this resource ( in case if machine has more than one NIC/address)
	QHostAddress getDiscoveryAddr() const;
	void setDiscoveryAddr(QHostAddress addr);

    virtual QString toString() const;
    QString toSearchString() const;


    void addNetworkStatus(unsigned long status);
    void removeNetworkStatus(unsigned long status);
    bool checkNetworkStatus(unsigned long status) const;
    void setNetworkStatus(unsigned long status);


    // return true if device conflicting with something else ( IP conflict )
    // this function makes sense to call only for resources in the same lan
    virtual bool conflicting();

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
    virtual bool updateMACAddress() = 0;

protected:
	QHostAddress m_hostAddr;
	QnMacAddress m_macAddress;

	QHostAddress m_localAddress; // address used to discover this resource ( in case if machine has more than one NIC/address

	unsigned long m_networkStatus;

	unsigned int m_networkTimeout;

private:
	QAuthenticator m_auth;

};

#endif // network_device_h_1249
