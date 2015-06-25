
#include "time_zones_model.h"

#include <QDebug>
#include <QTimeZone>

#include <base/server_info.h>

namespace
{
    enum { kInvalidIndex = -1 };
    
    const QByteArray utcIanaTemplate = "UTC";
    const QByteArray gmtIanaTemplate = "GMT";
    
    bool isDeprecatedTimeZone(const QByteArray &zoneIanaId) {
        return 
               zoneIanaId.contains(utcIanaTemplate)     // UTC+03:00
            || zoneIanaId.contains(gmtIanaTemplate)     // Etc/GMT+3
            ;
    }

    QString timeZoneNameWithOffset(const QTimeZone &timeZone, const QDateTime &atDateTime) {
        if (!timeZone.comment().isEmpty()) {
            Q_ASSERT_X(timeZone.comment().contains(utcIanaTemplate), Q_FUNC_INFO, "We are waiting for the (UTC+03:00) format");
            return timeZone.comment();
        }
        
        // Constructing format manually
        QString tzTemplate("(%1) %2");
        QString baseName(timeZone.id());      
        return tzTemplate.arg(timeZone.displayName(atDateTime, QTimeZone::OffsetName), baseName);
    }

    /** Some timezones have the same seconds offset but different UTC offset in
     *  their description (due to DST). So we have to adjust the order manually.
     *  Examples:
     *		(UTC-06:00) Central America
     *		(UTC-07:00) Mountain Time (US & Canada)
     *  both of these timezones have offset -21600 seconds now.
     */
    int timeZoneOrderKey(const QString &timeZoneName) {

        /* (UTC) timezones will always have zero key. */
        int key = 0;

        if (timeZoneName.contains(utcIanaTemplate)) {

            /* Parsing (UTC-06:00) part manually - that is much faster than regexp here. */
            int utcIdx = timeZoneName.indexOf(utcIanaTemplate);
            QString offsetHours = timeZoneName.mid(utcIdx + utcIanaTemplate.length(), 3);
            QString offsetMinutes = timeZoneName.mid(utcIdx + utcIanaTemplate.length() + 4, 2);

            if (offsetHours.startsWith(L'+')) {
                offsetHours.remove(0, 1);
                key += 60 * offsetHours.toInt();
                key += offsetMinutes.toInt();

            } else if (offsetHours.startsWith(L'-')) {
                offsetHours.remove(0, 1);
                key -= 60 * offsetHours.toInt();
                key -= offsetMinutes.toInt();
            }  
        }

        return key;
    }

    QStringList getBaseTimeZones()
    {
        const QList<QByteArray> timeZones = QTimeZone::availableTimeZoneIds();
            
        auto now = QDateTime::currentDateTime();
        QMultiMap<int, QString> tzByOffset;
        for(const auto &zoneIanaId: timeZones) {
            if (isDeprecatedTimeZone(zoneIanaId))
                continue;
            
            QTimeZone tz(zoneIanaId);
            QString displayName = timeZoneNameWithOffset(tz, now);

            tzByOffset.insert(  timeZoneOrderKey(displayName),
                                displayName);
        };
    
        QStringList result;
        for (auto &iter = tzByOffset.cbegin(); iter != tzByOffset.cend(); ++iter) {
            QString tzName(*iter);
            if (result.contains(tzName))
                continue;
            result.append(tzName);
        }
        
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



