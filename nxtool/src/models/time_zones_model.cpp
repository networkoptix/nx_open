
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
        TzInfo(const QByteArray &id, const QString &displayName):
            id(id), displayName(displayName) {}

        const QByteArray id;
        const QString displayName;
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
        //TODO: #ynikitenkov #tr
        m_timeZones.prepend(TzInfo(kDiffTimeZonesId, "<Different>"));
    }
    m_initIndex = timeZoneIndexById(m_initTimeZoneId);
    if (m_initIndex == kInvalidIndex)
    {
        //TODO: #ynikitenkov #tr
        m_timeZones.prepend(TzInfo(kUnknownTimeZoneId, "<Unknown>"));
        m_initIndex = 0;
    }
    //m_owner->dataChanged()
}

rtu::TimeZonesModel::Impl::~Impl()
{
}

bool rtu::TimeZonesModel::Impl::isValidValue(int index)
{
    if ((index >= m_timeZones.size()) || (index < 0))
        return false;
    
    const auto &value = m_timeZones[index];
    return ((value.id != kUnknownTimeZoneId) && (value.id != kDiffTimeZonesId));
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
        return tzInfo.id;
    default:
        break;
    }
    return QVariant();
}

void rtu::TimeZonesModel::Impl::initTimeZones() {
    const QList<QByteArray> timeZones = QTimeZone::availableTimeZoneIds();

    auto now = QDateTime::currentDateTime();
    QMultiMap<int, TzInfo> tzByOffset;
    for(const auto &zoneIanaId: timeZones) {
        if (isDeprecatedTimeZone(zoneIanaId))
            continue;

        QTimeZone tz(zoneIanaId);
        QString displayName = timeZoneNameWithOffset(tz, now);

        tzByOffset.insert(  timeZoneOrderKey(displayName),
                            TzInfo(zoneIanaId, displayName));
    };

   
    auto exists = [this](const QString &displayName) {
        return std::any_of(m_timeZones.cbegin(), m_timeZones.cend(), [displayName](const TzInfo &info) {
            return info.displayName == displayName;
        });
    };

    for (auto &iter = tzByOffset.cbegin(); iter != tzByOffset.cend(); ++iter) {
        TzInfo tzInfo(*iter);
        if (exists(tzInfo.displayName))
            continue;
        m_timeZones.append(tzInfo);
    }
}

int rtu::TimeZonesModel::Impl::timeZoneIndexById(const QByteArray &id) const {
    auto iter = std::find_if(m_timeZones.cbegin(), m_timeZones.cend(), [id](const TzInfo &info) {
        return info.id == id;
    });
    if (iter == m_timeZones.cend())
        return -1;
    return std::distance(m_timeZones.cbegin(), iter);
}

QByteArray rtu::TimeZonesModel::Impl::timeZoneIdByIndex(int index) const {
    if (index < 0 || index >= m_timeZones.size())
        return QByteArray();

    return m_timeZones[index].id;
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
