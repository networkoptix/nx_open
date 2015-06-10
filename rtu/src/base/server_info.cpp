
#include "server_info.h"

rtu::InterfaceInfo::InterfaceInfo()
    : name()
    , ip()
    , macAddress()
    , mask()
    , gateway()
    , useDHCP()
{}

rtu::InterfaceInfo::InterfaceInfo(bool initUseDHCP)
    : name()
    , ip()
    , macAddress()
    , mask()
    , gateway()
    , useDHCP(initUseDHCP ? Qt::Checked : Qt::Unchecked)
{}

rtu::InterfaceInfo::InterfaceInfo(QString initName
    , QString initIp
    , QString initMacAddress
    , QString initMask
    , QString initGateway
    , Qt::CheckState initUseDHCP)
    : name(initName)
    , ip(initIp)
    , macAddress(initMacAddress)
    , mask(initMask)
    , gateway(initGateway)
    , useDHCP(initUseDHCP)
{}

bool rtu::operator == (const BaseServerInfo &first
    , const BaseServerInfo &second)
{
    /// Do not compare hostAddress - it could be changed and it is Ok
    return ((first.flags == second.flags)
        && (first.id == second.id)
        && (first.name == second.name)
        && (first.port == second.port)
        && (first.systemName == second.systemName));
}

bool rtu::operator != (const BaseServerInfo &first
    , const BaseServerInfo &second)
{
    return !(first == second);
}

///

rtu::ExtraServerInfo::ExtraServerInfo()
    : password()
    , timestamp()
    , utcDateTime()
    , timeZone()
    , interfaces()
{
}

rtu::ExtraServerInfo::ExtraServerInfo(const QString &initPassword
    , const QDateTime &initTimestamp
    , const QDateTime &initUtcDateTime
    , const QTimeZone &initTimeZone
    , const InterfaceInfoList initInterfaces)
    : password(initPassword)
    , timestamp(initTimestamp)
    , utcDateTime(initUtcDateTime)
    , timeZone(initTimeZone)
    , interfaces(initInterfaces)
{
}

///

rtu::ServerInfo::ServerInfo()
    : m_base()
    , m_extra()
{
}

rtu::ServerInfo::ServerInfo(const ServerInfo &other)
    : m_base(other.baseInfo())
    , m_extra(!other.hasExtraInfo() ? nullptr 
        : new ExtraServerInfo(other.extraInfo()))
{
}

rtu::ServerInfo::ServerInfo(const BaseServerInfo &baseInfo)
    : m_base(baseInfo)
    , m_extra()
{
}

rtu::ServerInfo::~ServerInfo()
{
}

rtu::ServerInfo &rtu::ServerInfo::operator = (const ServerInfo &other)
{
    m_base = other.baseInfo();
    if (other.hasExtraInfo())
        setExtraInfo(other.extraInfo());
    return *this;
}

const rtu::BaseServerInfo &rtu::ServerInfo::baseInfo() const
{
    return m_base;
}

rtu::BaseServerInfo &rtu::ServerInfo::writableBaseInfo()
{
    return m_base;
}

void rtu::ServerInfo::setBaseInfo(const BaseServerInfo &baseInfo)
{
    m_base = baseInfo;
}

bool rtu::ServerInfo::hasExtraInfo() const
{
    return !m_extra.isNull();
}

const rtu::ExtraServerInfo &rtu::ServerInfo::extraInfo() const
{
    Q_ASSERT(!m_extra.isNull());
    return *m_extra;
}

rtu::ExtraServerInfo &rtu::ServerInfo::writableExtraInfo()
{
    Q_ASSERT(!m_extra.isNull());
    return *m_extra;
}

void rtu::ServerInfo::setExtraInfo(const ExtraServerInfo &extraInfo)
{
    if (hasExtraInfo())
    {
        *m_extra = extraInfo;
        return;
    }
    
    m_extra.reset(new ExtraServerInfo(extraInfo));
}

void rtu::ServerInfo::resetExtraInfo()
{
    m_extra.reset();
}
