
#include "server_info.h"


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

rtu::ServerInfo &rtu::ServerInfo::operator = (ServerInfo other)
{
    std::swap(*this, other);
    return *this;
}

const rtu::BaseServerInfo &rtu::ServerInfo::baseInfo() const
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
