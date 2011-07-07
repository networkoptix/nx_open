#include "network_resource.h"
#include "network/nettools.h"
#include "common/sleep.h"
#include "network/ping.h"

extern int ping_timeout ;

QnNetworkResource::QnNetworkResource():
mAfterRouter(false)
{
	addDeviceTypeFlag(QnResource::NETWORK);
}

bool QnNetworkResource::equalsTo(const  QnResourcePtr other) const
{

    QnNetworkResourcePtr nr = other.dynamicCast<QnNetworkResource>();

    if (!nr)
        return false;
    

    return (getIP() == nr->getIP() && getMAC() == nr->getMAC());
}

QHostAddress QnNetworkResource::getIP() const
{
	QMutexLocker mutex(&m_cs);
	return m_ip;
}

bool QnNetworkResource::setIP(const QHostAddress& ip, bool net )
{
	QMutexLocker mutex(&m_cs);
	m_ip = ip;
	return true;
}

void QnNetworkResource::setLocalAddr(QHostAddress addr)
{
	m_local_adssr = addr;
}

QString QnNetworkResource::getMAC() const
{
	return m_mac;
}

void  QnNetworkResource::setMAC(const QString& mac) 
{
	m_mac = mac;
}

void QnNetworkResource::setAfterRouter(bool after)
{
    mAfterRouter = after;
}

bool QnNetworkResource::isAfterRouter() const
{
    return mAfterRouter;
}

void QnNetworkResource::setAuth(const QString& user, QString password)
{
	QMutexLocker mutex(&m_cs);
	m_auth.setUser(user);
	m_auth.setPassword(password);
}

QAuthenticator QnNetworkResource::getAuth() const
{
	QMutexLocker mutex(&m_cs);
	return m_auth;
}

unsigned int QnNetworkResource::getHttpTimeout() 
{
	if (getStatus().checkFlag(QnResourceStatus::NOT_LOCAL)) 
		return 3000;
	else 
		return 1050;
}

QHostAddress QnNetworkResource::getDiscoveryAddr() const
{
	return m_local_adssr;
}

void QnNetworkResource::setDiscoveryAddr(QHostAddress addr)
{
	m_local_adssr = addr;
}

QString QnNetworkResource::toString() const
{
	QString result;
	QTextStream(&result) << getName() << "  " << getIP().toString() << "  " << getMAC();
	return result;
}

bool QnNetworkResource::conflicting()
{
    if (mAfterRouter)
        return false;

    QTime time;
	time.restart();
	CL_LOG(cl_logDEBUG2) cl_log.log("begining of QnNetworkResource::conflicting() ",  cl_logDEBUG2);

	QString mac = getMacByIP(getIP());

#ifndef _WIN32
	// If mac is empty or resolution is not implemented
	if (mac == "")
		return false;
#endif
	
	if (mac!=m_mac)// someone else has this IP
	{
		getStatus().setFlag(QnResourceStatus::CONFLICTING);
		return true;
	}

	CLSleep::msleep(10);

	CLPing ping;
	if (!ping.ping(getIP().toString(), 2, ping_timeout)) // I do know know how else to solve this problem. but getMacByIP do not creates any ARP record 
	{
		getStatus().setFlag(QnResourceStatus::CONFLICTING);
		return true;
	}

	mac = getMacByIP(getIP(), false); // just in case if ARP response from some else have delayed 

    

	if (mac!=m_mac && mac!="00-00-00-00-00-00")// someone else has this IP
	{
		getStatus().setFlag(QnResourceStatus::CONFLICTING);
		return true;
	}

    if (mac=="00-00-00-00-00-00")
    {
        CL_LOG(cl_logERROR) cl_log.log("00-00-00-00-00-00 mac record in OS arp( got it once on WIN7) table?!", cl_logERROR);
    }

	CL_LOG(cl_logDEBUG2) cl_log.log("end of  QnNetworkResource::conflicting(),  time elapsed: ", time.elapsed(), cl_logDEBUG2);
	return false;
}

