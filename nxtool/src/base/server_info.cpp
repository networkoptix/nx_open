
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

bool rtu::operator == (const rtu::InterfaceInfo &first
    , const rtu::InterfaceInfo &second)
{
    return ((first.ip == second.ip)
        && (first.macAddress == second.macAddress)
        && (first.mask == second.mask)
        && (first.gateway == second.gateway)
        && (first.dns == second.dns)
        && (first.useDHCP == second.useDHCP));
}

bool rtu::operator != (const rtu::InterfaceInfo &first
    , const rtu::InterfaceInfo &second)
{
    return !(first == second);
}

rtu::InterfaceInfo::InterfaceInfo(const QString &initName
    , const QString &initIp
    , const QString &initMacAddress
    , const QString &initMask
    , const QString &initGateway
    , const QString &initDns
    , Qt::CheckState initUseDHCP)
    : name(initName)
    , ip(initIp)
    , macAddress(initMacAddress)
    , mask(initMask)
    , gateway(initGateway)
    , dns(initDns)
    , useDHCP(initUseDHCP)
{}

bool rtu::operator == (const BaseServerInfo &first
    , const BaseServerInfo &second)
{
    /// Do not compare flags, hostAddress and visibleAddress - it is Ok
    return ((first.id == second.id)
        && (first.name == second.name)
        && (first.port == second.port)
        && (first.systemName == second.systemName)
        && (first.discoveredByHttp == second.discoveredByHttp));
}

bool rtu::operator != (const BaseServerInfo &first
    , const BaseServerInfo &second)
{
    return !(first == second);
}

///

rtu::ExtraServerInfo::ExtraServerInfo()
    : password()
    , timestampMs(0)
    , utcDateTimeMs(0)
    , timeZoneId()
    , interfaces()
{
}

rtu::ExtraServerInfo::ExtraServerInfo(const QString &initPassword
    , const qint64 &initTimestampMs
    , const qint64 &initUtcDateTimeMs
    , const QByteArray &initTimeZoneId
    , const InterfaceInfoList initInterfaces)
    : password(initPassword)
    , timestampMs(initTimestampMs)
    , utcDateTimeMs(initUtcDateTimeMs)
    , timeZoneId(initTimeZoneId)
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

rtu::ServerInfo::ServerInfo(const BaseServerInfo &baseInfo
    , const ExtraServerInfo &extraInfo)
    : m_base(baseInfo)
    , m_extra(new ExtraServerInfo(extraInfo))
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
