#include "network_resource.h"
#include "network/nettools.h"
#include "common/sleep.h"
#include "network/ping.h"

extern int ping_timeout ;

QnNetworkResource::QnNetworkResource():
m_networkStatus(0),
m_networkTimeout(2000)
{
    addFlag(QnResource::network);
}

QnNetworkResource::~QnNetworkResource()
{

}

bool QnNetworkResource::equalsTo(const  QnResourcePtr other) const
{
    QnNetworkResourcePtr nr = other.dynamicCast<QnNetworkResource>();

    if (!nr)
        return false;

    return (getHostAddress() == nr->getHostAddress() && getMAC() == nr->getMAC());
}

QHostAddress QnNetworkResource::getHostAddress() const
{
	QMutexLocker mutex(&m_mutex);
	return m_hostAddr;
}

bool QnNetworkResource::setHostAddress(const QHostAddress& ip, QnDomain domaun )
{
	QMutexLocker mutex(&m_mutex);
	m_hostAddr = ip;
	return true;
}

QnMacAddress QnNetworkResource::getMAC() const
{
    QMutexLocker mutex(&m_mutex);
	return m_macAddress;
}

void  QnNetworkResource::setMAC(QnMacAddress mac) 
{
    QMutexLocker mutex(&m_mutex);
	m_macAddress = mac;
}

void QnNetworkResource::setAuth(const QString& user, QString password)
{
	QMutexLocker mutex(&m_mutex);
	m_auth.setUser(user);
	m_auth.setPassword(password);
}

QAuthenticator QnNetworkResource::getAuth() const
{
	QMutexLocker mutex(&m_mutex);
	return m_auth;
}


QHostAddress QnNetworkResource::getDiscoveryAddr() const
{
    QMutexLocker mutex(&m_mutex);
	return m_localAddress;
}

void QnNetworkResource::setDiscoveryAddr(QHostAddress addr)
{
    QMutexLocker mutex(&m_mutex);
	m_localAddress = addr;
}

QString QnNetworkResource::toString() const
{
	QString result;
	QTextStream(&result) << getName() << "  " << getHostAddress().toString() << "  " << getMAC().toString();
	return result;
}

void QnNetworkResource::addNetworkStatus(unsigned long status)
{
    QMutexLocker mutex(&m_mutex);
    m_networkStatus |=  status;
}

void QnNetworkResource::removeNetworkStatus(unsigned long status)
{
    QMutexLocker mutex(&m_mutex);
    m_networkStatus &=  (~status);
}

bool QnNetworkResource::checkNetworkStatus(unsigned long status) const
{
    QMutexLocker mutex(&m_mutex);
    return m_networkStatus & status;
}

void QnNetworkResource::setNetworkTimeout(unsigned int timeout)
{
    QMutexLocker mutex(&m_mutex);
    m_networkTimeout = timeout;
}

unsigned int QnNetworkResource::getNetworkTimeout() const
{
    QMutexLocker mutex(&m_mutex);
    return m_networkTimeout;
}


bool QnNetworkResource::conflicting()
{

    if (checkNetworkStatus(QnNetworkResource::NotSameLAN))
        return false;

    QTime time;
	time.restart();
	CL_LOG(cl_logDEBUG2) cl_log.log("begining of QnNetworkResource::conflicting() ",  cl_logDEBUG2);

	QString mac = getMacByIP(getHostAddress());

#ifndef _WIN32
	// If mac is empty or resolution is not implemented
	if (mac == "")
		return false;
#endif
	
	if (mac!=m_macAddress.toString())// someone else has this IP
	{
		addNetworkStatus(QnNetworkResource::HasConflicts);
		return true;
	}

	QnSleep::msleep(10);

	CLPing ping;
	if (!ping.ping(getHostAddress().toString(), 2, ping_timeout)) // I do not know how else to solve this problem. but getMacByIP do not creates any ARP record 
	{
		addNetworkStatus(QnNetworkResource::HasConflicts);
		return true;
	}

	mac = getMacByIP(getHostAddress(), false); // just in case if ARP response from some else have delayed 

    

	if (mac!=m_macAddress.toString() && mac!="00-00-00-00-00-00")// someone else has this IP
	{
		addNetworkStatus(QnNetworkResource::HasConflicts);
		return true;
	}

    if (mac=="00-00-00-00-00-00")
    {
        CL_LOG(cl_logERROR) cl_log.log("00-00-00-00-00-00 mac record in OS arp( got it once on WIN7) table?!", cl_logERROR);
    }

	CL_LOG(cl_logDEBUG2) cl_log.log("end of  QnNetworkResource::conflicting(),  time elapsed: ", time.elapsed(), cl_logDEBUG2);
	return false;
}

