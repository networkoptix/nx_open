#include "network_resource.h"

#include "utils/network/nettools.h"
#include "utils/common/sleep.h"
#include "utils/network/ping.h"

Q_DECLARE_METATYPE(QHostAddress);
Q_DECLARE_METATYPE(QAuthenticator);

QnNetworkResource::QnNetworkResource()
    : QnResource(),
      m_networkStatus(0),
      m_networkTimeout(2000),
      m_authenticated(true)
{
    static volatile bool metaTypesInitialized = false;
    if (!metaTypesInitialized) {
        qRegisterMetaType<QHostAddress>();
        qRegisterMetaType<QnMacAddress>();
        qRegisterMetaType<QAuthenticator>();
        metaTypesInitialized = true;
    }

    addFlags(network);
}

QnNetworkResource::~QnNetworkResource()
{
}

void QnNetworkResource::deserialize(const QnResourceParameters& parameters)
{
    QnResource::deserialize(parameters);

    const char* MAC = "mac";
    //const char* URL = "url";
    const char* LOGIN = "login";
    const char* PASSWORD = "password";

    if (parameters.contains(QLatin1String(MAC)))
        setMAC(parameters[QLatin1String(MAC)]);

    if (parameters.contains(QLatin1String(LOGIN)) && parameters.contains(QLatin1String(PASSWORD)))
        setAuth(parameters[QLatin1String(LOGIN)], parameters[QLatin1String(PASSWORD)]);
}

QString QnNetworkResource::getUniqueId() const
{
    return getMAC().toString();
}

bool QnNetworkResource::equalsTo(const QnResourcePtr other) const
{
    QnNetworkResourcePtr nr = other.dynamicCast<QnNetworkResource>();
    if (!nr)
        return false;

    return (getHostAddress() == nr->getHostAddress() && getMAC() == nr->getMAC());
}

QHostAddress QnNetworkResource::getHostAddress() const
{
    //QMutexLocker mutex(&m_mutex);
    //return m_hostAddr;
    return QHostAddress(getUrl());
}

bool QnNetworkResource::setHostAddress(const QHostAddress &ip, QnDomain /*domain*/)
{
    //QMutexLocker mutex(&m_mutex);
    //m_hostAddr = ip;
    setUrl(ip.toString());
    return true;
}

QnMacAddress QnNetworkResource::getMAC() const
{
    QMutexLocker mutexLocker(&m_mutex);
    return m_macAddress;
}

void  QnNetworkResource::setMAC(const QnMacAddress &mac)
{
    QMutexLocker mutexLocker(&m_mutex);
    m_macAddress = mac;
}

void QnNetworkResource::setAuth(const QAuthenticator &auth)
{
    QMutexLocker mutexLocker(&m_mutex);
    m_auth = auth;
}

QAuthenticator QnNetworkResource::getAuth() const
{
    QMutexLocker mutexLocker(&m_mutex);
    return m_auth;
}

bool QnNetworkResource::isAuthenticated() const
{
    return m_authenticated;
}

void QnNetworkResource::setAuthenticated(bool auth)
{
    m_authenticated = auth;
}


QHostAddress QnNetworkResource::getDiscoveryAddr() const
{
    QMutexLocker mutexLocker(&m_mutex);
    return m_localAddress;
}

void QnNetworkResource::setDiscoveryAddr(QHostAddress addr)
{
    QMutexLocker mutexLocker(&m_mutex);
    m_localAddress = addr;
}

QString QnNetworkResource::toString() const
{
    QString result;
    QTextStream(&result) << getName() << "(" << getHostAddress().toString() << ") live";
    return result;
}

QString QnNetworkResource::toSearchString() const
{
    QString result;
    QTextStream(&result) << QnResource::toSearchString() << " " << getMAC().toString();
    return result;
}

void QnNetworkResource::addNetworkStatus(QnNetworkStatus status)
{
    QMutexLocker mutexLocker(&m_mutex);
    m_networkStatus |=  status;
}

void QnNetworkResource::removeNetworkStatus(QnNetworkStatus status)
{
    QMutexLocker mutexLocker(&m_mutex);
    m_networkStatus &=  (~status);
}

bool QnNetworkResource::checkNetworkStatus(QnNetworkStatus status) const
{
    QMutexLocker mutexLocker(&m_mutex);
    return m_networkStatus & status;
}

void QnNetworkResource::setNetworkStatus(QnNetworkStatus status)
{
    QMutexLocker mutexLocker(&m_mutex);
    m_networkStatus = status;
}

void QnNetworkResource::setNetworkTimeout(unsigned int timeout)
{
    QMutexLocker mutexLocker(&m_mutex);
    m_networkTimeout = timeout;
}

unsigned int QnNetworkResource::getNetworkTimeout() const
{
    QMutexLocker mutexLocker(&m_mutex);
    return m_networkTimeout;
}

void QnNetworkResource::updateInner(QnResourcePtr other)
{
    QMutexLocker mutexLocker(&m_mutex);
    QnResource::updateInner(other);
    QnNetworkResourcePtr other_casted = qSharedPointerDynamicCast<QnNetworkResource>(other);
    if (other_casted)
    {
        m_auth = other_casted->m_auth;
    }
}

bool QnNetworkResource::conflicting()
{
    if (checkNetworkStatus(QnNetworkResource::BadHostAddr))
        return false;

    QTime time;
    time.restart();
    CL_LOG(cl_logDEBUG2) cl_log.log("begining of QnNetworkResource::conflicting() ",  cl_logDEBUG2);

    QString mac = getMacByIP(getHostAddress());

//#ifndef _WIN32
    // If mac is empty or resolution is not implemented
    if (mac == "")
        return false;
//#endif

    if (mac!=m_macAddress.toString())// someone else has this IP
    {
        addNetworkStatus(QnNetworkResource::BadHostAddr);
        return true;
    }

    QnSleep::msleep(10);

    CLPing ping;
    if (!ping.ping(getHostAddress().toString(), 2, ping_timeout)) // I do not know how else to solve this problem. but getMacByIP do not creates any ARP record
    {
        //addNetworkStatus(QnNetworkResource::BadHostAddr);
        //return true;

        // some times ping does not work but cam is still is not conflicting
    }

    mac = getMacByIP(getHostAddress(), false); // just in case if ARP response from some else have delayed



    if (mac!=m_macAddress.toString() && mac!="00-00-00-00-00-00")// someone else has this IP
    {
        addNetworkStatus(QnNetworkResource::BadHostAddr);
        return true;
    }

    if (mac=="00-00-00-00-00-00")
    {
        CL_LOG(cl_logERROR) cl_log.log("00-00-00-00-00-00 mac record in OS arp( got it once on WIN7) table?!", cl_logERROR);
    }

    CL_LOG(cl_logDEBUG2) cl_log.log("end of  QnNetworkResource::conflicting(),  time elapsed: ", time.elapsed(), cl_logDEBUG2);
    return false;
}


/*
void QnNetworkResource::getDevicesBasicInfo(QnResourceMap& lst, int threads)
{
    // cannot make concurrent work with pointer CLDevice* ; => so extra steps needed

    cl_log.log(QLatin1String("Geting device info..."), cl_logDEBUG1);
    QTime time;
    time.start();


    QList<QnResourcePtr> local_list;
    foreach (QnResourcePtr res, lst.values())
    {
        QnNetworkResourcePtr netRes = qSharedPointerDynamicCast<QnNetworkResource>(res);
        if (netRes && !(netRes->checkNetworkStatus(QnNetworkResource::HasConflicts)))
            local_list << res.data();
    }


    QThreadPool* global = QThreadPool::globalInstance();
    for (int i = 0; i < threads; ++i ) global->releaseThread();
    QtConcurrent::blockingMap(local_list, std::mem_fun(&QnResource::getBaseInfo));
    for (int i = 0; i < threads; ++i )global->reserveThread();

    CL_LOG(cl_logDEBUG1)
    {
        cl_log.log(QLatin1String("Done. Time elapsed: "), time.elapsed(), cl_logDEBUG1);

        foreach(QnResourcePtr res, lst)
            cl_log.log(res->toString(), cl_logDEBUG1);

    }

}
/**/
