
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

    , m_softRestart()
    , m_osRestart()
    , m_factoryDefaults()
    , m_factoryDefaultsButNetwork()
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

bool rtu::Changeset::softRestart() const
{
    return m_softRestart;
}

bool rtu::Changeset::osRestart() const
{
    return m_osRestart;
}

bool rtu::Changeset::factoryDefaults() const
{
    return m_factoryDefaults;
}

bool rtu::Changeset::factoryDefaultsButNetwork() const
{
    return m_factoryDefaultsButNetwork;
}

void rtu::Changeset::addSystemChange(const QString &systemName)
{
    clearActions();
    m_systemName.reset(new QString(systemName));
}
        
void rtu::Changeset::addPasswordChange(const QString &password)
{
    clearActions();
    m_password.reset(new QString(password));
}
        
void rtu::Changeset::addPortChange(int port)
{
    clearActions();
    m_port.reset(new int(port));
}

void rtu::Changeset::addDHCPChange(const QString &name
    , bool useDHCP)
{
    clearActions();
    getItfUpdateInfo(name).useDHCP.reset(new bool(useDHCP));
}
        
void rtu::Changeset::addAddressChange(const QString &name
    , const QString &address)
{
    clearActions();
    getItfUpdateInfo(name).ip.reset(new QString(address));
}
        
void rtu::Changeset::addMaskChange(const QString &name
    , const QString &mask)
{
    clearActions();
    getItfUpdateInfo(name).mask.reset(new QString(mask));
}
        
void rtu::Changeset::addDNSChange(const QString &name
    , const QString &dns)
{
    clearActions();
    getItfUpdateInfo(name).dns.reset(new QString(dns));
}
        
void rtu::Changeset::addGatewayChange(const QString &name
    , const QString &gateway)
{
    clearActions();
    getItfUpdateInfo(name).gateway.reset(new QString(gateway));
}
        
void rtu::Changeset::addDateTimeChange(const QDate &date
    , const QTime &time
    , const QByteArray &timeZoneId)
{
    clearActions();
    qint64 utcTimeMs = msecondsFromEpoch(date, time, QTimeZone(timeZoneId));
    m_dateTime.reset(new DateTime(utcTimeMs, timeZoneId));
}

void rtu::Changeset::addSoftRestartAction()
{
    clear();
    m_softRestart = true;
}

void rtu::Changeset::addOsRestartAction()
{
    clear();
    m_osRestart = true;
}

void rtu::Changeset::addFactoryDefaultsAction()
{
    clear();
    m_factoryDefaults = true;
}

void rtu::Changeset::addFactoryDefaultsButNetworkAction()
{
    clear();
    m_factoryDefaultsButNetwork = true;
}

void rtu::Changeset::clear()
{
    m_port.reset();
    m_password.reset();
    m_systemName.reset();
    m_dateTime.reset();
    m_itfUpdateInfo.reset();

    clearActions();
}

void rtu::Changeset::clearActions()
{
    m_softRestart = false;
    m_osRestart = false;
    m_factoryDefaults = false;
    m_factoryDefaultsButNetwork = false;
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
