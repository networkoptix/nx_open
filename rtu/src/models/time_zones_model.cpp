
#include "time_zones_model.h"

#include <QDebug>
#include <QTimeZone>

#include <base/server_info.h>

namespace
{
    const QByteArray utcIanaTemplate = "UTC";
    
    QStringList getBaseTimeZones()
    {
        const QList<QByteArray> timeZones = QTimeZone::availableTimeZoneIds();
            
        QStringList result;
        for(const auto &zoneIanaId: timeZones)
        {
            if (!zoneIanaId.contains(utcIanaTemplate)
                || (zoneIanaId.size() == utcIanaTemplate.size()))
                result.push_back(zoneIanaId);
        };
    
        return result;
    }
    
    enum { kDifferentTimeZoneDefaultIndex = 0};
    const QString kDiffTimeZonesTag = QT_TR_NOOP("<Different>");
    const QStringList baseTimeZonesInfo = getBaseTimeZones();
    const QStringList timeZonesInfoWithDiff = []() -> QStringList
    {
        QStringList result = getBaseTimeZones();
        result.insert(kDifferentTimeZoneDefaultIndex, kDiffTimeZonesTag);
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
    int onCurrentIndexChanged(int index);
    
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
    , m_initIndex(m_withDiffTimeZoneItem ? kDifferentTimeZoneDefaultIndex 
        : baseTimeZonesInfo.indexOf(m_initSelectionTimeZone))
{
    const QStringList zones = (m_withDiffTimeZoneItem ? timeZonesInfoWithDiff : baseTimeZonesInfo);
    m_owner->setStringList(zones);
}

rtu::TimeZonesModel::Impl::~Impl()
{
}

int rtu::TimeZonesModel::Impl::onCurrentIndexChanged(int index)
{
    if (!m_withDiffTimeZoneItem || (index == kDifferentTimeZoneDefaultIndex))
        return false;
    
    /// removes <Different> value from available timezones
    m_owner->removeRow(kDifferentTimeZoneDefaultIndex);
    m_withDiffTimeZoneItem = false;
    
    enum { kInvalidIndex = -1 };
    m_initIndex = kInvalidIndex;
    emit m_owner->initIndexChanged();
    emit m_owner->currentTimeZoneIndexChanged();
    return index - 1;
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

int rtu::TimeZonesModel::onCurrentIndexChanged(int index)
{
    return m_impl->onCurrentIndexChanged(index);
}



