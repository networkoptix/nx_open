#ifndef network_device_h_1249
#define network_device_h_1249

//#include "param.h"
#include <QAuthenticator>
#include "../network/netstate.h"
#include "device.h"



class CLNetworkDevice : public CLDevice
{
	
public:

	CLNetworkDevice();


	QHostAddress getIP() const;

	// if net is true it will change real ip of device
	// return true if IP changed;
	// always return true if net = false
	virtual bool setIP(const QHostAddress& ip, bool net = true);

	void setLocalAddr(QHostAddress addr);

	QString getMAC() const;
	void  setMAC(const QString& mac);
	
	void setAuth(const QString& user, QString password);
	QAuthenticator getAuth() const;

	// address used to discover this divece ( in case if machine has more than one NIC/address)
	QHostAddress getDiscoveryAddr() const;
	
	// return true if device conflicting with smth else ( IP coflict )
	bool conflicting();


	virtual QString toString() const;

	//=============
	// some time we can find device, but cannot request additional information from it ( device has bad ip for example )
	// in this case we need to request additional information later.
	// unknownDevice - tels if we need that additional information 
	virtual bool unknownDevice() const = 0;

	// updateDevice requests the additional  information and return device with the same mac, ip but ready to use...
	virtual CLNetworkDevice* updateDevice()  = 0;
	//=============
	

protected:

	
	unsigned int getHttpTimeout() ;



protected:

	QHostAddress m_ip;
	QString m_mac;

	QHostAddress m_local_adssr; // address used to discover this divece ( in case if machine has more than one NIC/address

private:
	QAuthenticator m_auth;
	mutable QMutex m_auth_cs;

};


#endif // network_device_h_1249