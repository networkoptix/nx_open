#include "server_info.h"

namespace nx::vms::server::api {
    
InterfaceInfo::InterfaceInfo()
    : name()
    , ip()
    , macAddress()
    , mask()
    , gateway()
    , useDHCP()
{}

bool operator == (const InterfaceInfo &first
    , const InterfaceInfo &second)
{
    return ((first.ip == second.ip)
        && (first.macAddress == second.macAddress)
        && (first.mask == second.mask)
        && (first.gateway == second.gateway)
        && (first.dns == second.dns)
        && (first.useDHCP == second.useDHCP));
}

bool operator != (const InterfaceInfo &first
    , const InterfaceInfo &second)
{
    return !(first == second);
}

InterfaceInfo::InterfaceInfo(const QString &initName
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


BaseServerInfo::BaseServerInfo()
    : id()
    , runtimeId()
    , customization()

    , safeMode(false)
    , version()
    , os()
    , displayAddress()
    , flags(Constants::NoFlags)
        
    , name()
    , systemName()
    , hostAddress()
    , port(0)

    , httpAccessMethod(HttpAccessMethod::kTcp)
{
}


bool operator == (const BaseServerInfo &first
    , const BaseServerInfo &second)
{
    const bool firstHasHdd = (first.flags & Constants::HasHdd);
    const bool secondHasHdd = (second.flags & Constants::HasHdd);

    /// Do not compare flags, hostAddress and visibleAddress - it is Ok
    return ((first.id == second.id)
        && (first.name == second.name)
        && (first.port == second.port)
        && (first.systemName == second.systemName)
        && (first.version == second.version)
        && (first.runtimeId == second.runtimeId)
        && (first.safeMode == second.safeMode)
        && (firstHasHdd == secondHasHdd));
}

bool operator != (const BaseServerInfo &first
    , const BaseServerInfo &second)
{
    return !(first == second);
}

///

ExtraServerInfo::ExtraServerInfo()
    : password()
    , timestampMs(0)
    , utcDateTimeMs(0)
    , timeZoneId()
    , interfaces()
    , sysCommands(Constants::SystemCommand::NoCommands)
    , scriptNames()
{
}

ExtraServerInfo::ExtraServerInfo(const QString &initPassword
    , const qint64 &initTimestampMs
    , const qint64 &initUtcDateTimeMs
    , const QByteArray &initTimeZoneId
    , const InterfaceInfoList initInterfaces
    , const Constants::SystemCommands initSysCommands
    , const ScriptNames &initScriptNames)
    : password(initPassword)
    , timestampMs(initTimestampMs)
    , utcDateTimeMs(initUtcDateTimeMs)
    , timeZoneId(initTimeZoneId)
    , interfaces(initInterfaces)
    , sysCommands(initSysCommands)
    , scriptNames(initScriptNames)
{
}

///

ServerInfo::ServerInfo()
    : m_base()
    , m_extra()
{
}

ServerInfo::ServerInfo(const ServerInfo &other)
    : m_base(other.baseInfo())
    , m_extra(!other.hasExtraInfo() ? nullptr 
        : new ExtraServerInfo(other.extraInfo()))
{
}

ServerInfo::ServerInfo(const BaseServerInfo &baseInfo)
    : m_base(baseInfo)
    , m_extra()
{
}

ServerInfo::ServerInfo(const BaseServerInfo &baseInfo
    , const ExtraServerInfo &extraInfo)
    : m_base(baseInfo)
    , m_extra(new ExtraServerInfo(extraInfo))
{
}

ServerInfo::~ServerInfo()
{
}

ServerInfo &ServerInfo::operator = (const ServerInfo &other)
{
    m_base = other.baseInfo();
    if (other.hasExtraInfo())
        setExtraInfo(other.extraInfo());
    return *this;
}

const BaseServerInfo &ServerInfo::baseInfo() const
{
    return m_base;
}

BaseServerInfo &ServerInfo::writableBaseInfo()
{
    return m_base;
}

void ServerInfo::setBaseInfo(const BaseServerInfo &baseInfo)
{
    m_base = baseInfo;
}

bool ServerInfo::hasExtraInfo() const
{
    return !m_extra.isNull();
}

const ExtraServerInfo &ServerInfo::extraInfo() const
{
    Q_ASSERT(!m_extra.isNull());
    return *m_extra;
}

ExtraServerInfo &ServerInfo::writableExtraInfo()
{
    Q_ASSERT(!m_extra.isNull());
    return *m_extra;
}

void ServerInfo::setExtraInfo(const ExtraServerInfo &extraInfo)
{
    if (hasExtraInfo())
    {
        *m_extra = extraInfo;
        return;
    }
    
    m_extra.reset(new ExtraServerInfo(extraInfo));
}

void ServerInfo::resetExtraInfo()
{
    m_extra.reset();
}

} // namespace nx::vms::server::api
