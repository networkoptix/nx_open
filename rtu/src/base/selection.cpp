
#include "selection.h"

#include <base/server_info.h>
#include <helpers/time_helper.h>
#include <models/servers_selection_model.h>

namespace 
{
    template<typename ParameterType
        , typename GetterFunc
        , typename EqualsFunc>
    ParameterType getSelectionValue(const rtu::ServerInfoList &servers
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
    
    int calcPort(const rtu::ServerInfoList &servers)
    {
        enum { kDefaultPort = 0 };
        return getSelectionValue<int>(servers, kDefaultPort, kDefaultPort
            ,  std::equal_to<int>(), [](const rtu::ServerInfo &info)
        {
            return info.baseInfo().port; 
        });
    }
    
    rtu::Constants::ServerFlags calcFlags(const rtu::ServerInfoList &servers)
    {
        rtu::Constants::ServerFlags flags = rtu::Constants::AllFlags;
        for (const rtu::ServerInfo * const info: servers)
        {
            flags &= info->baseInfo().flags;
        }
        return flags;
    }

    QString calcSystemName(const rtu::ServerInfoList &servers)
    {
        if (servers.empty())
            return QString();
        
        static const QString kDifferentServerNames = "<different>";

        const auto &getter = [](const rtu::ServerInfo &info)
        {
            return info.baseInfo().systemName;
        };

        return getSelectionValue(servers, QString(), kDifferentServerNames
            , std::equal_to<QString>(), getter);
    }

    QString calcPassword(const rtu::ServerInfoList &servers)
    {
        if (servers.empty())
            return QString();

        static const QString kDifferentPasswords = QT_TR_NOOP("<Different passwords>");
        const auto &getter = [](const rtu::ServerInfo &info) -> QString
        {
            return (info.hasExtraInfo() ? info.extraInfo().password : QString()); 
        };
        
        return getSelectionValue(servers, QString()
            , kDifferentPasswords, std::equal_to<QString>(), getter);
    }

    QDateTime calcDateTime(const rtu::ServerInfoList &servers
        , rtu::Constants::ServerFlags flags)
    {
        if (servers.empty())
            return QDateTime();
        
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        const auto &getter = [now](const rtu::ServerInfo &info) -> QDateTime
        {
            if (!info.hasExtraInfo())
                return QDateTime();

            const qint64 dateMsecs = info.extraInfo().utcDateTime.toMSecsSinceEpoch();
            const qint64 timestamp = info.extraInfo().timestamp.toMSecsSinceEpoch();
            return QDateTime::fromMSecsSinceEpoch(dateMsecs + (now - timestamp));
        };

        const int selectedCount = servers.size();
        if (selectedCount == kSingleServerSelectionCount)
        {
            const rtu::ServerInfo &firstInfo = **servers.begin();
            const QDateTime time = getter(firstInfo);
            if (!time.isValid() || time.isNull())
                return QDateTime();

            return rtu::convertUtcToTimeZone(time, firstInfo.extraInfo().timeZone);
        }
    
        static const auto &eq = [](const QDateTime &first, const QDateTime &second) -> bool
        {
            enum { kEps = 130 * 1000};    /// 30 seconds
            return (std::abs(first.toMSecsSinceEpoch() - second.toMSecsSinceEpoch()) < kEps);
        };
    
    
        const QDateTime result = getSelectionValue<QDateTime>(
            servers, QDateTime(), QDateTime(), eq, getter);

        const QByteArray tz = getSelectionValue<QByteArray>(servers, QByteArray(), QByteArray()
            , std::equal_to<QByteArray>(), [](const rtu::ServerInfo &info) -> QByteArray
        {
            return (info.hasExtraInfo() ? info.extraInfo().timeZone.id() : QByteArray());
        });

        return (result.isNull() || !result.isValid() || tz.isEmpty() ?
            QDateTime() : rtu::convertUtcToTimeZone(result, QTimeZone(tz)));
    }

    
    bool calcEditableInterfaces(const rtu::ServerInfoList &servers)
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
}

///

struct rtu::Selection::Snapshot
{
    int count;
    int port;
    rtu::Constants::ServerFlags flags;
    QString systemName;
    QString password;
    QDateTime dateTime;
    bool editableInterfaces;
    
    Snapshot(rtu::ServersSelectionModel *model);
};

rtu::Selection::Snapshot::Snapshot(rtu::ServersSelectionModel *model)

    : count(model->selectedCount())
    , port(calcPort(model->selectedServers()))
    , flags(calcFlags(model->selectedServers()))
    , systemName(calcSystemName(model->selectedServers()))
    , password(calcPassword(model->selectedServers()))
    , dateTime(calcDateTime(model->selectedServers(), flags))
    , editableInterfaces(calcEditableInterfaces(model->selectedServers()))
{}

///

rtu::Selection::Selection(ServersSelectionModel *selectionModel
    , QObject *parent)
    : QObject(parent)
    , m_snapshot(new Snapshot(selectionModel))
{}

rtu::Selection::~Selection()
{}

int rtu::Selection::count()
{
    return m_snapshot->count;
}

int rtu::Selection::port()
{
    return m_snapshot->port;
}

int rtu::Selection::flags()
{
    return static_cast<int>(m_snapshot->flags);
}

QString rtu::Selection::systemName()
{
    return m_snapshot->systemName;
}

QString rtu::Selection::password()
{
    return m_snapshot->password;
}

QDateTime rtu::Selection::dateTime()
{
    return m_snapshot->dateTime;
}

bool rtu::Selection::editableInterfaces()
{
    return m_snapshot->editableInterfaces;
}
