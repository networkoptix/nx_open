
#include "changeset.h"

#include <QHostAddress>

#include <helpers/qml_helpers.h>

namespace api = nx::vms::server::api;

namespace
{
    const auto kChangesProgressTextDesc = QStringLiteral("Applying changes... (%1/%2)");
    const auto kRestartProgressTextDesc = QStringLiteral("Restarting servers... (%1/%2)");
    const auto kRestoreFactoryProgressTextDesc = QStringLiteral("Restoring servers to factory defaults... (%1/%2)");

    const auto kMinimizedChangesProgressTextDesc = QStringLiteral("Applying changes...");
    const auto kMinimizedRestartProgressTextDesc = QStringLiteral("Restarting...");
    const auto kMinimizedRestoreProgressTextDesc = QStringLiteral("Restoring...");
}

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

const api::Client::ItfUpdateInfoContainerPtr &rtu::Changeset::itfUpdateInfo() const
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

QString rtu::Changeset::getProgressText() const
{
    if (m_factoryDefaults || m_factoryDefaultsButNetwork)
        return kRestoreFactoryProgressTextDesc;
    else if (m_osRestart || m_softRestart)
        return kRestartProgressTextDesc;
    
    return kChangesProgressTextDesc;
}

QString rtu::Changeset::getMinimizedProgressText() const
{
    if (m_factoryDefaults || m_factoryDefaultsButNetwork)
        return kMinimizedRestoreProgressTextDesc;
    else if (m_osRestart || m_softRestart)
        return kMinimizedRestartProgressTextDesc;
    
    return kMinimizedChangesProgressTextDesc;
}

void rtu::Changeset::addSystemChange(const QString &systemName)
{
    preparePropChangesData();
    m_systemName.reset(new QString(systemName.trimmed()));
}
        
void rtu::Changeset::addPasswordChange(const QString &password)
{
    preparePropChangesData();
    m_password.reset(new QString(password.trimmed()));
}
        
void rtu::Changeset::addPortChange(int port)
{
    preparePropChangesData();
    m_port.reset(new int(port));
}

void rtu::Changeset::addDHCPChange(const QString &name
    , bool useDHCP)
{
    preparePropChangesData();
    getItfUpdateInfo(name).useDHCP.reset(new bool(useDHCP));
}
        
void rtu::Changeset::addAddressChange(const QString &name
    , const QString &address)
{
    preparePropChangesData();
    getItfUpdateInfo(name).ip.reset(new QString(QHostAddress(address).toString()));
}
        
void rtu::Changeset::addMaskChange(const QString &name
    , const QString &mask)
{
    preparePropChangesData();
    getItfUpdateInfo(name).mask.reset(new QString(QHostAddress(mask).toString()));
}
        
void rtu::Changeset::addDNSChange(const QString &name
    , const QString &dns)
{
    preparePropChangesData();
    getItfUpdateInfo(name).dns.reset(new QString(QHostAddress(dns).toString()));
}
        
void rtu::Changeset::addGatewayChange(const QString &name
    , const QString &gateway)
{
    preparePropChangesData();
    getItfUpdateInfo(name).gateway.reset(new QString(QHostAddress(gateway).toString()));
}
        
void rtu::Changeset::addDateTimeChange(const QDate &date
    , const QTime &time
    , const QByteArray &timeZoneId)
{
    preparePropChangesData();
    qint64 utcTimeMs = msecondsFromEpoch(date, time, QTimeZone(timeZoneId));
    m_dateTime.reset(new DateTime(utcTimeMs, timeZoneId));
}

void rtu::Changeset::addSoftRestartAction()
{
    preapareActionData();
    m_softRestart = true;
}

void rtu::Changeset::addOsRestartAction()
{
    preapareActionData();
    m_osRestart = true;
}

void rtu::Changeset::addFactoryDefaultsAction()
{
    preapareActionData();
    m_factoryDefaults = true;
}

void rtu::Changeset::addFactoryDefaultsButNetworkAction()
{
    preapareActionData();
    m_factoryDefaultsButNetwork = true;
}

void rtu::Changeset::preapareActionData()
{
    m_port.reset();
    m_password.reset();
    m_systemName.reset();
    m_dateTime.reset();
    m_itfUpdateInfo.reset();

    preparePropChangesData();
}

void rtu::Changeset::preparePropChangesData()
{
    m_softRestart = false;
    m_osRestart = false;
    m_factoryDefaults = false;
    m_factoryDefaultsButNetwork = false;
}

api::Client::ItfUpdateInfo &rtu::Changeset::getItfUpdateInfo(const QString &name)
{
    api::Client::ItfUpdateInfoContainer &itfUpdate = [this]() -> api::Client::ItfUpdateInfoContainer&
    {
        if (!m_itfUpdateInfo)
            m_itfUpdateInfo = std::make_shared<api::Client::ItfUpdateInfoContainer>();
        return *m_itfUpdateInfo;
    }();

    const auto &it = std::find_if(itfUpdate.begin(), itfUpdate.end()
        , [&name](const api::Client::ItfUpdateInfo &itf)
    {
        return (itf.name == name);
    });
    
    if (it == itfUpdate.end())
    {
        itfUpdate.push_back(api::Client::ItfUpdateInfo(name));
        return itfUpdate.back();
    }
    return *it;
}
