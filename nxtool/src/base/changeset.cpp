
#include "changeset.h"

#include <base/requests.h>

#include <helpers/qml_helpers.h>
#include <helpers/time_helper.h>

rtu::ChangesetPointer rtu::Changeset::create()
{
    return ChangesetPointer(new Changeset());
}

rtu::Changeset::Changeset()
    : QObject(rtu::helpers::qml_objects_parent())

    , m_port()
    , m_password()
    , m_systemName()
    , m_dateTime()
    , m_itfUpdateInfo()
{
}

rtu::Changeset::~Changeset()
{
}

const rtu::IntPointer &rtu::Changeset::port() const
{
    return m_port;
}

const rtu::StringPointer &rtu::Changeset::password() const
{
    return m_password;
}
        
const rtu::StringPointer &rtu::Changeset::systemName() const
{
    return m_systemName;
}

const rtu::DateTimePointer &rtu::Changeset::dateTime() const
{
    return m_dateTime;
}

const rtu::ItfUpdateInfoContainerPointer &rtu::Changeset::itfUpdateInfo() const
{
    return m_itfUpdateInfo;
}

void rtu::Changeset::addSystemChange(const QString &systemName)
{
    m_systemName.reset(new QString(systemName));
}
        
void rtu::Changeset::addPasswordChange(const QString &password)
{
    m_password.reset(new QString(password));
}
        
void rtu::Changeset::addPortChange(int port)
{
    m_port.reset(new int(port));
}

void rtu::Changeset::addDHCPChange(const QString &name
    , bool useDHCP)
{
    getItfUpdateInfo(name).useDHCP.reset(new bool(useDHCP));
}
        
void rtu::Changeset::addAddressChange(const QString &name
    , const QString &address)
{
    getItfUpdateInfo(name).ip.reset(new QString(address));
}
        
void rtu::Changeset::addMaskChange(const QString &name
    , const QString &mask)
{
    getItfUpdateInfo(name).mask.reset(new QString(mask));
}
        
void rtu::Changeset::addDNSChange(const QString &name
    , const QString &dns)
{
    getItfUpdateInfo(name).dns.reset(new QString(dns));
}
        
void rtu::Changeset::addGatewayChange(const QString &name
    , const QString &gateway)
{
    getItfUpdateInfo(name).gateway.reset(new QString(gateway));
}
        
void rtu::Changeset::addDateTimeChange(const QDate &date
    , const QTime &time
    , const QByteArray &timeZoneId)
{
    qint64 utcTimeMs = msecondsFromEpoch(date, time, QTimeZone(timeZoneId));
    m_dateTime.reset(new DateTime(utcTimeMs, timeZoneId));
}

rtu::ItfUpdateInfo &rtu::Changeset::getItfUpdateInfo(const QString &name)
{
    ItfUpdateInfoContainer &itfUpdate = [this]()-> ItfUpdateInfoContainer&
    {
        if (!m_itfUpdateInfo)
            m_itfUpdateInfo = std::make_shared<ItfUpdateInfoContainer>();
        return *m_itfUpdateInfo;
    }();

    const auto &it = std::find_if(itfUpdate.begin(), itfUpdate.end()
        , [&name](const ItfUpdateInfo &itf)
    {
        return (itf.name == name);
    });
    
    if (it == itfUpdate.end())
    {
        itfUpdate.push_back(ItfUpdateInfo(name));
        return itfUpdate.back();
    }
    return *it;
}
