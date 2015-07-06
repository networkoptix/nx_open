
#include "time_zones_model.h"

#include <QDebug>
#include <QTimeZone>

#include <base/server_info.h>

namespace
{
    enum { kInvalidIndex = -1 };
    
    const QByteArray utcIanaTemplate = "UTC";
    const QByteArray gmtIanaTemplate = "GMT";
    QStringList getBaseTimeZones()
    {
        const QList<QByteArray> timeZones = QTimeZone::availableTimeZoneIds();
            
        QStringList result;
        for(const auto &zoneIanaId: timeZones)
        {
            if (!zoneIanaId.contains(utcIanaTemplate) && !zoneIanaId.contains(gmtIanaTemplate))
                result.push_back(zoneIanaId);
        };
    
        return result;
    }
    
    const QString kDiffTimeZonesTag = QT_TR_NOOP("<Different>");
    const QString kUnknownTimeZoneTag = QT_TR_NOOP("<Unknown>");
    const QStringList baseTimeZonesInfo = getBaseTimeZones();
    const QStringList timeZonesInfoWithDiff = []() -> QStringList
    {
        QStringList result = getBaseTimeZones();
        result.push_back(kDiffTimeZonesTag);
        return result;
    }();
    
    
    QString selectionTimeZone(const rtu::ServerInfoList &servers)
    {
        if (servers.empty())
            return kDiffTimeZonesTag;
        
        const QString timeZone = (**servers.begin()).extraInfo().timeZone.id();
        for (rtu::ServerInfo *info: servers)
        {
            if (timeZone != info->extraInfo().timeZone.id())
                return kDiffTimeZonesTag;
        }
        return timeZone;
    }
}

class rtu::TimeZonesModel::Impl : public QObject
{
public:
    Impl(rtu::TimeZonesModel *owner
        , const rtu::ServerInfoList &selectedServers);
    
    virtual ~Impl();
    
public:
    bool isValidValue(int index);
    
    int initIndex() const;
    
    int currentTimeZoneIndex() const;

private:
    rtu::TimeZonesModel * const m_owner;
    QString m_initSelectionTimeZone;
    bool m_withDiffTimeZoneItem;
    int m_initIndex;
};

rtu::TimeZonesModel::Impl::Impl(rtu::TimeZonesModel *owner
    , const rtu::ServerInfoList &selectedServers)
    : QObject(owner)
    , m_owner(owner)
    , m_initSelectionTimeZone(selectionTimeZone(selectedServers))
    , m_withDiffTimeZoneItem(m_initSelectionTimeZone == kDiffTimeZonesTag)
    , m_initIndex(m_withDiffTimeZoneItem ? timeZonesInfoWithDiff.indexOf(kDiffTimeZonesTag) 
        : baseTimeZonesInfo.indexOf(m_initSelectionTimeZone))
{
    QStringList zones = (m_withDiffTimeZoneItem ? timeZonesInfoWithDiff : baseTimeZonesInfo);
    if (m_initIndex == kInvalidIndex)
    {
        zones.push_back(kUnknownTimeZoneTag);
        m_initIndex = zones.size() - 1;
    }
    m_owner->setStringList(zones);
}

rtu::TimeZonesModel::Impl::~Impl()
{
}

bool rtu::TimeZonesModel::Impl::isValidValue(int index)
{
    const QStringList &zones = m_owner->stringList();
    if ((index >= zones.size()) || (index < 0))
        return false;
    
    const QString &value = zones.at(index);
    return ((value != kUnknownTimeZoneTag) && (value != kDiffTimeZonesTag));
}

int rtu::TimeZonesModel::Impl::initIndex() const
{
    return m_initIndex;
}

int rtu::TimeZonesModel::Impl::currentTimeZoneIndex() const
{
    QStringList zones = m_owner->stringList();
    const QString currentTimeZoneId =  QDateTime::currentDateTime().timeZone().id();
    const int result = zones.indexOf(currentTimeZoneId);

    return result;
}

///

rtu::TimeZonesModel::TimeZonesModel(const ServerInfoList &selectedServers
    , QObject *parent)
    : QStringListModel(parent)
    , m_impl(new Impl(this, selectedServers))
{
    
}

rtu::TimeZonesModel::~TimeZonesModel()
{
}

int rtu::TimeZonesModel::initIndex() const
{
    return m_impl->initIndex();
}

int rtu::TimeZonesModel::currentTimeZoneIndex()
{
    return m_impl->currentTimeZoneIndex();
}

bool rtu::TimeZonesModel::isValidValue(int index)
{
    return m_impl->isValidValue(index);
}



