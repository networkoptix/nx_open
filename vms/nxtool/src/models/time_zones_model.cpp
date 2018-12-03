
#include "time_zones_model.h"

#include <QTimeZone>

#include <helpers/time_helper.h>
#include <helpers/model_change_helper.h>

namespace api = nx::vms::server::api;

namespace
{
    enum { kInvalidIndex = -1 };

    const QByteArray utcIanaTemplate = "UTC";
    const QByteArray gmtIanaTemplate = "GMT";
    const QString utcKeyword = "Universal";

    const QList<QByteArray> utcAliases = [] {
        return QList<QByteArray>()
            << "Etc/GMT"
            << "Etc/GMT+0"
            << "Etc/UCT"
            << "Etc/Universal"
            << "Etc/UTC"
            << "GMT"
            << "GMT+0"
            << "GMT0"
            << "GMT-0"
            << "Greenwich"
            << "UCT"
            << "Universal";
    }();

    bool isDeprecatedTimeZone(const QByteArray &zoneIanaId) {
        return
               zoneIanaId.contains(utcIanaTemplate)     // UTC+03:00
            || zoneIanaId.contains(gmtIanaTemplate)     // Etc/GMT+3
            ;
    }

    const QByteArray kDiffTimeZonesId = "<Different>";
    const QByteArray kUnknownTimeZoneId = "<Unknown>";


    QByteArray selectionTimeZoneId(const api::ServerInfoPtrContainer &servers)
    {
        if (servers.empty() || !servers.first()->hasExtraInfo())
            return kDiffTimeZonesId;

        const QByteArray timeZoneId = servers.first()->extraInfo().timeZoneId;
        for (api::ServerInfo *info: servers)
        {
            if (!info->hasExtraInfo() || (timeZoneId != info->extraInfo().timeZoneId))
                return kDiffTimeZonesId;
        }
        return timeZoneId;
    }
}

class rtu::TimeZonesModel::Impl
{
public:
    Impl(rtu::TimeZonesModel *owner
        , const api::ServerInfoPtrContainer &selectedServers);

    Impl(rtu::TimeZonesModel *owner
        , const Impl &other);

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
        TzInfo(const QByteArray &id, const QString &displayName, int standardTimeOffset):
            displayName(displayName),
            standardTimeOffset(standardTimeOffset)
        {
            ids.append(id);
            Q_ASSERT_X(!id.isEmpty(), Q_FUNC_INFO, "IANA id should not be empty here.");
        }

        QByteArray primaryId() const {
            return !ids.isEmpty() ? ids.first() : QByteArray();
        }

        QString displayName;
        int standardTimeOffset;
        QList<QByteArray> ids;
    };

    rtu::TimeZonesModel * const m_owner;
    QList<TzInfo> m_timeZones;

    QByteArray m_initTimeZoneId;
    int m_initIndex;
};

rtu::TimeZonesModel::Impl::Impl(rtu::TimeZonesModel *owner
    , const api::ServerInfoPtrContainer &selectedServers)
    : m_owner(owner)
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

rtu::TimeZonesModel::Impl::Impl(rtu::TimeZonesModel *owner
    , const Impl &other)
    : m_owner(owner)
    , m_timeZones(other.m_timeZones)
    , m_initTimeZoneId(other.m_initTimeZoneId)
    , m_initIndex(other.m_initIndex)
{
}


rtu::TimeZonesModel::Impl::~Impl()
{
}

bool rtu::TimeZonesModel::Impl::isValidValue(int index)
{
    if ((index >= m_timeZones.size()) || (index < 0))
        return false;

    const auto &value = m_timeZones[index];
    return ((value.primaryId() != kUnknownTimeZoneId) && (value.primaryId() != kDiffTimeZonesId));
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
        return tzInfo.primaryId();
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
            existing->ids.append(zoneIanaId);
        else
            m_timeZones.append(TzInfo(zoneIanaId, displayName, tz.standardTimeOffset(now)));
    };

    std::sort(m_timeZones.begin(), m_timeZones.end(), [now](const TzInfo &l, const TzInfo &r) {
        return l.standardTimeOffset == r.standardTimeOffset
            ? l.displayName < r.displayName
            : l.standardTimeOffset < r.standardTimeOffset;
    });


    auto addCustomTimeZone = [this](const QByteArray &zoneIanaId, int standardTimeOffset) {

        bool alreadyExists = std::any_of(m_timeZones.cbegin(), m_timeZones.cend(), [zoneIanaId](const TzInfo &info) {
            return info.ids.contains(zoneIanaId);
        });

        if (alreadyExists)
            return;

        /* Try to find with keyword first, otherwise - any with the same offset. */
        auto existingUtc = std::find_if(m_timeZones.begin(), m_timeZones.end(), [standardTimeOffset](const TzInfo &info) {
            return info.standardTimeOffset == standardTimeOffset && info.displayName.contains(utcKeyword);
        });
        if (existingUtc == m_timeZones.end()) {
            existingUtc = std::find_if(m_timeZones.begin(), m_timeZones.end(), [standardTimeOffset](const TzInfo &info) {
                return info.standardTimeOffset == standardTimeOffset;
            });
        }

        if (existingUtc != m_timeZones.end())
            existingUtc->ids.append(zoneIanaId);
    };

    /* Add deprecated time zones as secondary id's. */
    for(const auto &zoneIanaId: timeZones)
        if (isDeprecatedTimeZone(zoneIanaId))
            addCustomTimeZone(zoneIanaId, QTimeZone(zoneIanaId).standardTimeOffset(now));

    /* Manually register all IANA UTC links. */
    for (const auto &utcAlias: utcAliases)
        addCustomTimeZone(utcAlias, 0);
}

int rtu::TimeZonesModel::Impl::timeZoneIndexById(const QByteArray &id) const {
    auto iter = std::find_if(m_timeZones.cbegin(), m_timeZones.cend(), [id](const TzInfo &info) {
        return info.ids.contains(id);
    });
    if (iter == m_timeZones.cend())
        return -1;
    return std::distance(m_timeZones.cbegin(), iter);
}

QByteArray rtu::TimeZonesModel::Impl::timeZoneIdByIndex(int index) const {
    if (index < 0 || index >= m_timeZones.size())
        return QByteArray();

    return m_timeZones[index].primaryId();
}

///

rtu::TimeZonesModel::TimeZonesModel(const api::ServerInfoPtrContainer &selectedServers
    , QObject *parent)
    : QAbstractListModel(parent)
    , m_helper(CREATE_MODEL_CHANGE_HELPER(this))
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

int rtu::TimeZonesModel::currentTimeZoneIndex() const
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

void rtu::TimeZonesModel::resetTo(TimeZonesModel *source)
{
    if (!source)
        return;

    const auto resetHelper = m_helper->resetModelGuard();
    Q_UNUSED(resetHelper);

    m_impl.reset(new Impl(this, *source->m_impl));
}


QByteArray rtu::TimeZonesModel::timeZoneIdByIndex(int index) const {
    return m_impl->timeZoneIdByIndex(index);
}
