
#include "selection.h"

#include <base/server_info.h>
#include <helpers/time_helper.h>
#include <helpers/qml_helpers.h>
#include <models/time_zones_model.h>
#include <models/ip_settings_model.h>
#include <models/servers_selection_model.h>

namespace 
{
    QElapsedTimer timestampTimer;

    enum { kEps = 30 * 1000};    /// 30 seconds

    template<typename ParameterType
        , typename GetterFunc
        , typename EqualsFunc>
    ParameterType getSelectionValue(const rtu::ServerInfoPtrContainer &servers
        , const ParameterType &emptyValue
        , const ParameterType &differentValue
        , const EqualsFunc &eqFunc
        , const GetterFunc &getter)
    {
        if (servers.empty())
            return emptyValue;
        
        const rtu::ServerInfo * const firstInfo = *servers.begin();
        const ParameterType &value = getter(*firstInfo);
        const auto itDifferentValue = std::find_if(servers.begin(), servers.end()
            , [&value, &getter, &eqFunc](const rtu::ServerInfo *info) { return (!eqFunc(getter(*info), value));});
        return (itDifferentValue == servers.end() ? value : differentValue);
    }

    enum { kSingleServerSelectionCount = 1 };
    
    int calcPort(const rtu::ServerInfoPtrContainer &servers)
    {
        enum { kDefaultPort = 0 };
        return getSelectionValue<int>(servers, kDefaultPort, kDefaultPort
            ,  std::equal_to<int>(), [](const rtu::ServerInfo &info)
        {
            return info.baseInfo().port; 
        });
    }
    
    template<typename ReturnValue, typename InitValue>
    ReturnValue calcFlags(const rtu::ServerInfoPtrContainer &servers, InitValue allSelectedValue)
    {
        ReturnValue flags = allSelectedValue;
        for (const rtu::ServerInfo * const info: servers)
        {
            flags &= info->baseInfo().flags;
        }
        return flags;
    }

    Qt::CheckState calcDHCPState(const rtu::ServerInfoPtrContainer &servers)
    {
        return getSelectionValue<Qt::CheckState>(servers, Qt::Unchecked, Qt::PartiallyChecked
            , std::equal_to<Qt::CheckState>(), [](const rtu::ServerInfo &info)
        {
            if (!info.hasExtraInfo() || info.extraInfo().interfaces.empty())
                return Qt::Unchecked;
            
            const rtu::InterfaceInfoList &interfaces = info.extraInfo().interfaces;
            Qt::CheckState firstVal = interfaces.begin()->useDHCP;
            for (const rtu::InterfaceInfo &itfInfo: interfaces)
            {
                if (itfInfo.useDHCP != firstVal)
                    return Qt::PartiallyChecked;
            }
            return firstVal;
        });
    }
    
    QString calcSystemName(const rtu::ServerInfoPtrContainer &servers)
    {
        if (servers.empty())
            return QString();
        
        static const QString kDifferentServerNames = QString();

        const auto &getter = [](const rtu::ServerInfo &info)
        {
            return info.baseInfo().systemName;
        };

        return getSelectionValue(servers, QString(), kDifferentServerNames
            , std::equal_to<QString>(), getter);
    }

    QString calcPassword(const rtu::ServerInfoPtrContainer &servers)
    {
        if (servers.empty())
            return QString();

        static const QString kDifferentPasswords = QString();
        const auto &getter = [](const rtu::ServerInfo &info) -> QString
        {
            return (info.hasExtraInfo() ? info.extraInfo().password : QString()); 
        };
        
        return getSelectionValue(servers, QString()
            , kDifferentPasswords, std::equal_to<QString>(), getter);
    }

    QDateTime calcDateTime(const rtu::ServerInfoPtrContainer &servers)
    {
        QDateTime defaultDateTime = QDateTime();

        if (servers.empty())
            return defaultDateTime;
        
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        const auto &getter = [now](const rtu::ServerInfo &info) -> qint64
        {
            if (!info.hasExtraInfo())
                return 0;

            const qint64 dateMsecs = info.extraInfo().utcDateTimeMs;
            const qint64 timestamp = info.extraInfo().timestampMs;
            return dateMsecs + (now - timestamp);
        };

        const int selectedCount = servers.size();
        if (selectedCount == kSingleServerSelectionCount)
        {
            const rtu::ServerInfo &firstInfo = **servers.begin();
            qint64 utcTimeMs = getter(firstInfo);
            if (utcTimeMs <= 0)
                return defaultDateTime;

            return rtu::convertUtcToTimeZone(utcTimeMs, QTimeZone(firstInfo.extraInfo().timeZoneId));
        }
    
        static const auto &eq = [](qint64 first, qint64 second) -> bool
        {
            return (std::abs(first - second) < kEps);
        };
    
        const qint64 utcTimeMs = getSelectionValue<qint64>(
            servers, 0, 0, eq, getter);

        const QByteArray tz = getSelectionValue<QByteArray>(servers, QByteArray(), QByteArray()
            , std::equal_to<QByteArray>(), [](const rtu::ServerInfo &info) -> QByteArray
        {
            return (info.hasExtraInfo() ? info.extraInfo().timeZoneId : QByteArray());
        });

        return (utcTimeMs <= 0  || tz.isEmpty() ?
            defaultDateTime : rtu::convertUtcToTimeZone(utcTimeMs, QTimeZone(tz)));
    }

    
    bool calcEditableInterfaces(const rtu::ServerInfoPtrContainer &servers)
    {
        if (servers.empty())
            return false;
        
        if (servers.size() == kSingleServerSelectionCount)
            return true;
        
        enum 
        {
            kEmptyIps = 0
            , kDifferentIpsCount = -1 
            , kSingleIp = 1
        };
        
        const int ipsCount = getSelectionValue<int>(servers, kEmptyIps, kDifferentIpsCount
            , std::equal_to<int>(), [](const rtu::ServerInfo &info) -> int
        {
            return (info.hasExtraInfo() ? info.extraInfo().interfaces.size() : kEmptyIps);  
        });
        
        return (ipsCount == kSingleIp ? true : false);
    }

    /// Make sense only if all servers has one interfaces. 
    /// Calculated value will not be used in other cases
    bool calcHasEmptyAddresses(const rtu::ServerInfoPtrContainer &servers)
    {
        if (servers.empty())
            return false;

        const bool aggregatedHasEmptyIps = getSelectionValue<bool>(servers, false, true
            , std::equal_to<bool>(), [](const rtu::ServerInfo &info) -> bool
        {
            if (!info.hasExtraInfo())
                return false;

            const auto &interfaces = info.extraInfo().interfaces;
            if (interfaces.empty())
                return false;
            
            return interfaces.first().ip.isEmpty();
        });

        return aggregatedHasEmptyIps;
    }

    typedef std::unique_ptr<rtu::IpSettingsModel> IpSettingsModelPtr;

    IpSettingsModelPtr makeItfModel(bool editableInterfacess
        , const rtu::ServerInfoPtrContainer &servers)
    {
        /// Editability checked at calcEditableInterfaces.

        if (!editableInterfacess || servers.empty())
            return std::move(IpSettingsModelPtr(new rtu::IpSettingsModel(0
                , rtu::InterfaceInfoList(), rtu::helpers::qml_objects_parent())));

        const auto &firstServer = servers.front();
        if (servers.size() == kSingleServerSelectionCount)
        {
            const rtu::InterfaceInfoList &itfs = (firstServer->hasExtraInfo() 
                ? firstServer->extraInfo().interfaces : rtu::InterfaceInfoList());
            return std::move(IpSettingsModelPtr(new rtu::IpSettingsModel(kSingleServerSelectionCount
                , itfs, rtu::helpers::qml_objects_parent())));
        }

        /// All servers have only one interfaces due to calcEditableInterfaces() result check before call
        const auto dhcpCheckedState = getSelectionValue<Qt::CheckState>(servers, Qt::Unchecked, Qt::PartiallyChecked
            , std::equal_to<Qt::CheckState>(), [](const rtu::ServerInfo &info) -> Qt::CheckState
        {
            return (!info.hasExtraInfo() ? Qt::PartiallyChecked : info.extraInfo().interfaces.first().useDHCP);
        });

        static const QString kEmptyAddress; 

        const auto maskGetter = [](const rtu::ServerInfo &info) -> QString
            { return (info.hasExtraInfo() ? info.extraInfo().interfaces.first().mask : kEmptyAddress); };
        const auto dnsGetter = [](const rtu::ServerInfo &info) -> QString
            { return (info.hasExtraInfo() ? info.extraInfo().interfaces.first().dns : kEmptyAddress); };
        const auto gatewayGetter = [](const rtu::ServerInfo &info) -> QString
            { return (info.hasExtraInfo() ? info.extraInfo().interfaces.first().gateway: kEmptyAddress); };

        const auto mask = getSelectionValue<QString>(servers, kEmptyAddress, kEmptyAddress
            , std::equal_to<QString>(), maskGetter);
        const auto gateway = getSelectionValue<QString>(servers, kEmptyAddress, kEmptyAddress
            , std::equal_to<QString>(), gatewayGetter);
        const auto dns = getSelectionValue<QString>(servers, kEmptyAddress, kEmptyAddress
            , std::equal_to<QString>(), dnsGetter);

        rtu::InterfaceInfoList addresses;

        addresses.push_back(rtu::InterfaceInfo(rtu::Selection::kMultipleSelectionItfName
            , kEmptyAddress, kEmptyAddress              /// Ip and Mac case: addresses are always different for servers
            , mask, gateway, dns, dhcpCheckedState ));
        
        return std::move(IpSettingsModelPtr(new rtu::IpSettingsModel(servers.size()
            , addresses, rtu::helpers::qml_objects_parent())));
    }

    bool itfsAreDifferent(const IpSettingsModelPtr &first
        , const IpSettingsModelPtr &second)
    {
        const auto firstInterfaces = first->interfaces();
        const auto secondInterfaces = second->interfaces();

        return (firstInterfaces != secondInterfaces);
    }

    typedef std::unique_ptr<rtu::TimeZonesModel> TimeZonesModelPtr;
    bool timeZonesAreDifferent(const TimeZonesModelPtr &current
        , const TimeZonesModelPtr &other)
    {
        const auto currentIndex = current->index(current->currentTimeZoneIndex());
        const auto currentTz = current->data(currentIndex, Qt::DisplayRole).toString();

        const auto otherIndex = other->index(current->currentTimeZoneIndex());
        const auto otherTz = other->data(otherIndex, Qt::DisplayRole).toString();

        return (currentTz != otherTz);
    }
}

///

const QString rtu::Selection::kMultipleSelectionItfName = QStringLiteral("no_matter");

struct rtu::Selection::Snapshot
{
    int count;

    rtu::Constants::ServerFlags flags;

    int port;
    Qt::CheckState dhcpState;
    bool editableInterfaces;
    bool hasEmptyIps;
    IpSettingsModelPtr itfModel;

    QString systemName;
    QString password;

    qint64 timestamp;
    QDateTime dateTime;
    TimeZonesModelPtr timeZonesModel;
    
    rtu::Constants::ExtraServerFlags extraFlags;

    Snapshot(rtu::ServersSelectionModel *model);
};

rtu::Selection::Snapshot::Snapshot(rtu::ServersSelectionModel *model)

    : count(model->selectedCount())

    , flags(calcFlags<rtu::Constants::ServerFlags>(
        model->selectedServers(), rtu::Constants::ServerFlag::AllFlags))

    , port(calcPort(model->selectedServers()))
    , dhcpState(calcDHCPState(model->selectedServers()))
    , editableInterfaces(calcEditableInterfaces(model->selectedServers()))
    , hasEmptyIps(calcHasEmptyAddresses(model->selectedServers()))
    , itfModel(makeItfModel(editableInterfaces, model->selectedServers()))

    , systemName(calcSystemName(model->selectedServers()))
    , password(calcPassword(model->selectedServers()))
    
    , timestamp(timestampTimer.elapsed())
    , dateTime(calcDateTime(model->selectedServers()))
    , timeZonesModel(new TimeZonesModel(model->selectedServers(), rtu::helpers::qml_objects_parent()))

    , extraFlags(calcFlags<rtu::Constants::ExtraServerFlags>(
        model->selectedServers(), rtu::Constants::ExtraServerFlag::AllExtraFlags))
{
}

///

rtu::Selection::Selection(ServersSelectionModel *selectionModel
    , QObject *parent)
    : QObject(parent)
    , m_snapshot(new Snapshot(selectionModel))
{
}

rtu::Selection::~Selection()
{}


void rtu::Selection::updateSelection(ServersSelectionModel *selectionModel)
{
    const auto otherSnapshot = Snapshot(selectionModel);

    bool emitDateTimeChanged = (m_snapshot->count != otherSnapshot.count);
    bool emitSystemSettingsChanged = (m_snapshot->count != otherSnapshot.count);
    bool emitInterfacesChanged = (m_snapshot->count != otherSnapshot.count);
    bool emitActionsChanged = (m_snapshot->count != otherSnapshot.count);

    /// Check differences in password/system name settings
    if (!emitSystemSettingsChanged
        && ((m_snapshot->systemName != otherSnapshot.systemName)
            || (m_snapshot->password != otherSnapshot.password)))
    {
        emitSystemSettingsChanged = true;
    }

    /// Check differences in password/system name settings
    if (!emitDateTimeChanged
        && ((std::abs(m_snapshot->dateTime.toMSecsSinceEpoch() - otherSnapshot.dateTime.toMSecsSinceEpoch()) > kEps)
            || (timeZonesAreDifferent(m_snapshot->timeZonesModel, otherSnapshot.timeZonesModel))))
    {
        emitDateTimeChanged = true;
    }

    /// Check differences in port/interfaces settings
    if (!emitInterfacesChanged && 
        ((m_snapshot->port != otherSnapshot.port)
            || (m_snapshot->editableInterfaces != otherSnapshot.editableInterfaces)
            || itfsAreDifferent(m_snapshot->itfModel, otherSnapshot.itfModel)))
    {
        emitInterfacesChanged = true;
    }

    /// Check differences in extra flags (used for actions availability)
    if (!emitActionsChanged && (m_snapshot->extraFlags != otherSnapshot.extraFlags))
    {
        emitActionsChanged = true;
    }

    m_snapshot->count = otherSnapshot.count;

    m_snapshot->flags = otherSnapshot.flags;

    m_snapshot->port = otherSnapshot.port;
    m_snapshot->dhcpState = otherSnapshot.dhcpState;
    m_snapshot->editableInterfaces = otherSnapshot.editableInterfaces;
    m_snapshot->hasEmptyIps = otherSnapshot.hasEmptyIps;
    m_snapshot->itfModel->resetTo(otherSnapshot.itfModel.get());

    m_snapshot->systemName = otherSnapshot.systemName;
    m_snapshot->password = otherSnapshot.password;

    m_snapshot->timestamp = otherSnapshot.timestamp;
    m_snapshot->dateTime = otherSnapshot.dateTime;
    m_snapshot->timeZonesModel->resetTo(otherSnapshot.timeZonesModel.get());

    m_snapshot->extraFlags = otherSnapshot.extraFlags;

    if (emitDateTimeChanged)
        emit dateTimeSettingsChanged();

    if (emitInterfacesChanged)
        emit interfaceSettingsChanged();

    if (emitSystemSettingsChanged)
        emit systemSettingsChanged();

    if (emitActionsChanged)
        emit actionsSettingsChanged();
}

int rtu::Selection::count() const
{
    return m_snapshot->count;
}

int rtu::Selection::port() const
{
    return m_snapshot->port;
}

int rtu::Selection::flags() const
{
    return static_cast<int>(m_snapshot->flags);
}

int rtu::Selection::extraFlags() const
{
    return static_cast<int>(m_snapshot->extraFlags);
}

int rtu::Selection::dhcpState() const
{
    return static_cast<int>(m_snapshot->dhcpState);
}

const QString &rtu::Selection::systemName() const
{
    return m_snapshot->systemName;
}

const QString &rtu::Selection::password() const
{
    return m_snapshot->password;
}

QDateTime rtu::Selection::dateTime() const
{
    if (!m_snapshot->dateTime.isValid())
        return m_snapshot->dateTime;
    
    return m_snapshot->dateTime.addMSecs(timestampTimer.elapsed() - m_snapshot->timestamp);
}

QObject *rtu::Selection::timeZonesModel()
{
    return m_snapshot->timeZonesModel.get();
}

bool rtu::Selection::editableInterfaces() const
{
    return m_snapshot->editableInterfaces;
}

bool rtu::Selection::hasEmptyIps() const
{
    return m_snapshot->hasEmptyIps;
}

QObject *rtu::Selection::itfSettingsModel()
{
    return m_snapshot->itfModel.get();
}

