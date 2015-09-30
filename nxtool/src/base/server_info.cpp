
#include "server_info.h"

rtu::InterfaceInfo::InterfaceInfo()
    : name()
    , ip()
    , macAddress()
    , mask()
    , gateway()
    , useDHCP()
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


rtu::BaseServerInfo::BaseServerInfo()
    : id()
    , runtimeId()

    , safeMode(false)
    , version()
    , displayAddress()
    , flags(Constants::NoFlags)
        
    , name()
    , systemName()
    , hostAddress()
    , port(0)

    , accessibleByHttp(true)
{
}


bool rtu::operator == (const BaseServerInfo &first
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
    , sysCommands(Constants::SystemCommand::NoCommands)
    , scriptNames()
{
}

rtu::ExtraServerInfo::ExtraServerInfo(const QString &initPassword
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
