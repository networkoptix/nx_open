#ifndef network_device_h_1249
#define network_device_h_1249

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
    virtual bool setHostAddress(const QHostAddress& ip, QnDomain domaun = QnDomainPhysical);

	QnMacAddress getMAC() const;
	void  setMAC(QnMacAddress mac);

	void setAuth(const QString& user, QString password);
	QAuthenticator getAuth() const;

	// address used to discover this resource ( in case if machine has more than one NIC/address)
	QHostAddress getDiscoveryAddr() const;
	void setDiscoveryAddr(QHostAddress addr);

    virtual QString toString() const;

    void addNetworkStatus(unsigned long status);
    void removeNetworkStatus(unsigned long status);
    bool checkNetworkStatus(unsigned long status) const;


	// return true if device conflicting with something else ( IP conflict )
    // this function makes sense to call only for resources in the same lan
	virtual bool conflicting();

    // all data readers and any sockets will use this number as timeout value in ms 
    void setNetworkTimeout(unsigned int timeout);
    virtual unsigned int getNetworkTimeout() const;
	

	//=============
	// some time we can find device, but cannot request additional information from it ( device has bad ip for example )
	// in this case we need to request additional information later.
	// unknownResource - tels if we need that additional information 
	virtual bool unknownResource() const = 0;

	// updateResource requests the additional  information and return device with the same mac, ip but ready to use...
	virtual QnNetworkResourcePtr updateResource()  = 0;
	//=============

    // sometimes resource is not in your lan, and it might be not pingable from one hand 
    // but from other hand it's still might replay to standard requests
    // so this is the way to find out do we have to change ip address
    virtual bool isResourceAccessible()  = 0;

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
