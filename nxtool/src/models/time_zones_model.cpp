
#include "time_zones_model.h"

#include <QDebug>
#include <QTimeZone>

#include <base/server_info.h>
#include <helpers/time_helper.h>

namespace
{
    enum { kInvalidIndex = -1 };
    
    const QByteArray utcIanaTemplate = "UTC";
    const QByteArray gmtIanaTemplate = "GMT";
    const QString utcKeyword = "Universal";
    
    bool isDeprecatedTimeZone(const QByteArray &zoneIanaId) {
        return 
               zoneIanaId.contains(utcIanaTemplate)     // UTC+03:00
            || zoneIanaId.contains(gmtIanaTemplate)     // Etc/GMT+3
            ;
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

    bool timeZoneLessThan(const QString &l, const QString &r) {
        int leftKey = timeZoneOrderKey(l);
        int rightKey = timeZoneOrderKey(r);
        if (leftKey != rightKey)
            return leftKey < rightKey;
        return l < r;
    }
    
    const QByteArray kDiffTimeZonesId = "<Different>";
    const QByteArray kUnknownTimeZoneId = "<Unknown>";
  
    
    QByteArray selectionTimeZoneId(const rtu::ServerInfoList &servers)
    {
        if (servers.empty())
            return kDiffTimeZonesId;
        
        const QByteArray timeZoneId = servers.first()->extraInfo().timeZoneId;
        for (rtu::ServerInfo *info: servers)
        {
            //TODO: #ynikitenkov make sure extraInfo exists here
            if (timeZoneId != info->extraInfo().timeZoneId)
                return kDiffTimeZonesId;
        }
        return timeZoneId;
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

    int rowCount() const;

    QVariant data(const QModelIndex &index, int role) const;

    QByteArray timeZoneIdByIndex(int index) const;
    int timeZoneIndexById(const QByteArray &id) const;

private:
    void initTimeZones();  

private:
    struct TzInfo {
        TzInfo(const QByteArray &id, const QString &displayName, int offsetFromUtc):
            primaryId(id),
            displayName(displayName),
            offsetFromUtc(offsetFromUtc)
        {}

        QByteArray primaryId;
        QString displayName;
        int offsetFromUtc;        
        QList<QByteArray> secondaryIds;
    };

    rtu::TimeZonesModel * const m_owner;
    QList<TzInfo> m_timeZones;

    QByteArray m_initTimeZoneId;
    int m_initIndex;
};

rtu::TimeZonesModel::Impl::Impl(rtu::TimeZonesModel *owner
    , const rtu::ServerInfoList &selectedServers)
    : QObject(owner)
    , m_owner(owner)
    , m_timeZones()
    , m_initTimeZoneId(selectionTimeZoneId(selectedServers))
    , m_initIndex(0)
{
    initTimeZones();

    if (m_initTimeZoneId == kDiffTimeZonesId) {
        m_timeZones.prepend(TzInfo(kDiffTimeZonesId, tr("<Different>"), std::numeric_limits<int>::max()));
    }
    m_initIndex = timeZoneIndexById(m_initTimeZoneId);
    if (m_initIndex == kInvalidIndex)
    {
        m_timeZones.prepend(TzInfo(kUnknownTimeZoneId, tr("<Unknown>"), std::numeric_limits<int>::min()));
        m_initIndex = 0;
    }
}

rtu::TimeZonesModel::Impl::~Impl()
{
}

bool rtu::TimeZonesModel::Impl::isValidValue(int index)
{
    if ((index >= m_timeZones.size()) || (index < 0))
        return false;
    
    const auto &value = m_timeZones[index];
    return ((value.primaryId != kUnknownTimeZoneId) && (value.primaryId != kDiffTimeZonesId));
}

int rtu::TimeZonesModel::Impl::initIndex() const
{
    return m_initIndex;
}

int rtu::TimeZonesModel::Impl::currentTimeZoneIndex() const
{
    const QByteArray currentTimeZoneId = QDateTime::currentDateTime().timeZone().id();
    return timeZoneIndexById(currentTimeZoneId);
}

int rtu::TimeZonesModel::Impl::rowCount() const {
    return m_timeZones.size();
}

QVariant rtu::TimeZonesModel::Impl::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount())
        return QVariant();

    const auto tzInfo = m_timeZones[index.row()];

    switch(role) {
    case Qt::DisplayRole:
    case Qt::StatusTipRole:
    case Qt::WhatsThisRole:
    case Qt::AccessibleTextRole:
    case Qt::AccessibleDescriptionRole:
    case Qt::ToolTipRole:
    case Qt::EditRole:
        return tzInfo.displayName;
    case Qt::UserRole:
        return tzInfo.primaryId;
    default:
        break;
    }
    return QVariant();
}

void rtu::TimeZonesModel::Impl::initTimeZones() {
    const QList<QByteArray> timeZones = QTimeZone::availableTimeZoneIds();

    auto now = QDateTime::currentDateTime();
    for(const auto &zoneIanaId: timeZones) {
        if (isDeprecatedTimeZone(zoneIanaId))
            continue;

        QTimeZone tz(zoneIanaId);
        QString displayName = timeZoneNameWithOffset(tz, now);

         /* Combine timezones with same display names. */
        auto existing = std::find_if(m_timeZones.begin(), m_timeZones.end(), [&displayName](const TzInfo &info) {
            return info.displayName == displayName;
        });

        if (existing != m_timeZones.end())
            existing->secondaryIds.append(zoneIanaId);
        else
            m_timeZones.append(TzInfo(zoneIanaId, displayName, tz.offsetFromUtc(now)));
    };

    std::sort(m_timeZones.begin(), m_timeZones.end(), [](const TzInfo &l, const TzInfo &r) {
        return timeZoneLessThan(l.displayName, r.displayName);
    });


    /* Add deprecated time zones as secondary id's. */
    for(const auto &zoneIanaId: timeZones) {
        if (!isDeprecatedTimeZone(zoneIanaId))
            continue;

        QTimeZone tz(zoneIanaId);
        int offsetFromUts = tz.offsetFromUtc(now);

        /* Try to find with keyword first, otherwise - any with the same offset. */
        auto existingUtc = std::find_if(m_timeZones.begin(), m_timeZones.end(), [offsetFromUts](const TzInfo &info) {
            return info.offsetFromUtc == offsetFromUts && info.displayName.contains(utcKeyword);
        });
        if (existingUtc == m_timeZones.end()) {
            existingUtc = std::find_if(m_timeZones.begin(), m_timeZones.end(), [offsetFromUts](const TzInfo &info) {
                return info.offsetFromUtc == offsetFromUts;
            });
        }

        if (existingUtc != m_timeZones.end())
            existingUtc->secondaryIds.append(zoneIanaId);
    }
}

int rtu::TimeZonesModel::Impl::timeZoneIndexById(const QByteArray &id) const {
    auto iter = std::find_if(m_timeZones.cbegin(), m_timeZones.cend(), [id](const TzInfo &info) {
        return info.primaryId == id || info.secondaryIds.contains(id);
    });
    if (iter == m_timeZones.cend())
        return -1;
    return std::distance(m_timeZones.cbegin(), iter);
}

QByteArray rtu::TimeZonesModel::Impl::timeZoneIdByIndex(int index) const {
    if (index < 0 || index >= m_timeZones.size())
        return QByteArray();

    return m_timeZones[index].primaryId;
}

///

rtu::TimeZonesModel::TimeZonesModel(const ServerInfoList &selectedServers
    , QObject *parent)
    : QAbstractListModel(parent)
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

int rtu::TimeZonesModel::rowCount(const QModelIndex &parent /*= QModelIndex()*/) const {
    /* This model do not have sub-rows. */
    Q_ASSERT_X(!parent.isValid(), Q_FUNC_INFO, "Only null index should come here.");
    if (parent.isValid())
        return 0;
    return m_impl->rowCount();
}

QVariant rtu::TimeZonesModel::data(const QModelIndex &index , int role /*= Qt::DisplayRole*/) const {
    return m_impl->data(index, role);
}

QByteArray rtu::TimeZonesModel::timeZoneIdByIndex(int index) const {
    return m_impl->timeZoneIdByIndex(index);
}
