
#include "selection.h"

#include <base/server_info.h>
#include <helpers/time_helper.h>
#include <models/servers_selection_model.h>

namespace 
{
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
    
    rtu::Constants::ServerFlags calcFlags(const rtu::ServerInfoPtrContainer &servers)
    {
        rtu::Constants::ServerFlags flags = rtu::Constants::AllFlags;
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

    QDateTime calcDateTime(const rtu::ServerInfoPtrContainer &servers
        , rtu::Constants::ServerFlags flags)
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
            enum { kEps = 30 * 1000};    /// 30 seconds
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
            return info.extraInfo().interfaces.size(); 
        });
        
        return (ipsCount == kSingleIp ? true : false);
    }

    rtu::InterfaceInfoList calcAggregatedInterfaces(const rtu::ServerInfoPtrContainer &servers)
    {
        /// Editability checked at calcEditableInterfaces.

        if (servers.empty())
            return rtu::InterfaceInfoList();

        const auto &firstServer = servers.front();
        if (servers.size() == kSingleServerSelectionCount)
            return (firstServer->hasExtraInfo() ? firstServer->extraInfo().interfaces : rtu::InterfaceInfoList());

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

        static const QString kFakeItfName = QLatin1String("no_matter");
        rtu::InterfaceInfoList addresses;

        addresses.push_back(rtu::InterfaceInfo(kFakeItfName
            , kEmptyAddress, kEmptyAddress              /// Ip and Mac case: addresses are always different for servers
            , mask, gateway, dns, dhcpCheckedState ));
        return addresses;
    }
}

///

struct rtu::Selection::Snapshot
{
    int count;
    int port;
    rtu::Constants::ServerFlags flags;
    Qt::CheckState dhcpState;
    QString systemName;
    QString password;
    QDateTime dateTime;
    bool editableInterfaces;
    InterfaceInfoList aggregatedInterfaces;

    Snapshot(rtu::ServersSelectionModel *model);
};

rtu::Selection::Snapshot::Snapshot(rtu::ServersSelectionModel *model)

    : count(model->selectedCount())
    , port(calcPort(model->selectedServers()))
    , flags(calcFlags(model->selectedServers()))
    , dhcpState(calcDHCPState(model->selectedServers()))
    , systemName(calcSystemName(model->selectedServers()))
    , password(calcPassword(model->selectedServers()))
    , dateTime(calcDateTime(model->selectedServers(), flags))
    , editableInterfaces(calcEditableInterfaces(model->selectedServers()))
    , aggregatedInterfaces(editableInterfaces ? calcAggregatedInterfaces(model->selectedServers()) : InterfaceInfoList())
{}

///

rtu::Selection::Selection(ServersSelectionModel *selectionModel
    , QObject *parent)
    : QObject(parent)
    , m_snapshot(new Snapshot(selectionModel))
{}

rtu::Selection::~Selection()
{}

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

const QDateTime &rtu::Selection::dateTime() const
{
    return m_snapshot->dateTime;
}

bool rtu::Selection::editableInterfaces() const
{
    return m_snapshot->editableInterfaces;
}

rtu::InterfaceInfoList rtu::Selection::aggregatedInterfaces()
{
    return m_snapshot->aggregatedInterfaces;
}
