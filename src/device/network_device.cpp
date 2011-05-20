#include "network_device.h"

#include "../base/log.h"
#include "../network/nettools.h"
#include "../network/ping.h"
#include "../base/sleep.h"
#include "device_command_processor.h"

extern int ping_timeout ;

CLNetworkDevice::CLNetworkDevice():
mAfterRouter(false)
{
	addDeviceTypeFlag(CLDevice::NETWORK);
}

QHostAddress CLNetworkDevice::getIP() const
{
	QMutexLocker mutex(&m_cs);
	return m_ip;
}

bool CLNetworkDevice::setIP(const QHostAddress& ip, bool net )
{
	QMutexLocker mutex(&m_cs);
	m_ip = ip;
	return true;
}

void CLNetworkDevice::setLocalAddr(QHostAddress addr)
{
	m_local_adssr = addr;
}

QString CLNetworkDevice::getMAC() const
{
	return m_mac;
}

void  CLNetworkDevice::setMAC(const QString& mac) 
{
	m_mac = mac;
}

void CLNetworkDevice::setAfterRouter(bool after)
{
    mAfterRouter = after;
}

bool CLNetworkDevice::isAfterRouter() const
{
    return mAfterRouter;
}

void CLNetworkDevice::setAuth(const QString& user, QString password)
{
	QMutexLocker mutex(&m_cs);
	m_auth.setUser(user);
	m_auth.setPassword(password);
}

QAuthenticator CLNetworkDevice::getAuth() const
{
	QMutexLocker mutex(&m_cs);
	return m_auth;
}

unsigned int CLNetworkDevice::getHttpTimeout() 
{
	if (getStatus().checkFlag(CLDeviceStatus::NOT_LOCAL)) 
		return 3000;
	else 
		return 1050;
}

QHostAddress CLNetworkDevice::getDiscoveryAddr() const
{
	return m_local_adssr;
}

void CLNetworkDevice::setDiscoveryAddr(QHostAddress addr)
{
	m_local_adssr = addr;
}

QString CLNetworkDevice::toString() const
{
	QString result;
	QTextStream(&result) << getName() << "  " << getIP().toString() << "  " << getMAC();
	return result;
}

bool CLNetworkDevice::conflicting()
{
    if (mAfterRouter)
        return false;
	
    QTime time;
	time.restart();
	CL_LOG(cl_logDEBUG2) cl_log.log("begining of CLNetworkDevice::conflicting() ",  cl_logDEBUG2);

	QString mac = getMacByIP(getIP());

#ifndef _WIN32
	// If mac is empty or resolution is not implemented
	if (mac == "")
		return false;
#endif
	
	if (mac!=m_mac)// someone else has this IP
	{
		getStatus().setFlag(CLDeviceStatus::CONFLICTING);
		return true;
	}

	CLSleep::msleep(10);

	CLPing ping;
	if (!ping.ping(getIP().toString(), 2, ping_timeout)) // I do know know how else to solve this problem. but getMacByIP do not creates any ARP record 
	{
		getStatus().setFlag(CLDeviceStatus::CONFLICTING);
		return true;
	}

	mac = getMacByIP(getIP(), false); // just in case if ARP response from some else have delayed 

    

	if (mac!=m_mac && mac!="00-00-00-00-00-00")// someone else has this IP
	{
		getStatus().setFlag(CLDeviceStatus::CONFLICTING);
		return true;
	}

    if (mac!="00-00-00-00-00-00")
    {
        CL_LOG(cl_logERROR) cl_log.log("00-00-00-00-00-00 mac record in OS arp( got it once on WIN7) table?!", cl_logERROR);
    }

	CL_LOG(cl_logDEBUG2) cl_log.log("end of  CLNetworkDevice::conflicting(),  time elapsed: ", time.elapsed(), cl_logDEBUG2);
	return false;
}

