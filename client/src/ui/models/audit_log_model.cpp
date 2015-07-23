#include "audit_log_model.h"

#include <cassert>

#include <QtGui/QPalette>

#include <utils/common/warnings.h>
#include <utils/common/synctime.h>
#include "core/resource/resource.h"
#include "core/resource_management/resource_pool.h"
#include "ui/style/resource_icon_cache.h"
#include "client/client_globals.h"
#include <utils/math/math.h>
#include <plugins/resource/server_camera/server_camera.h>
#include "client/client_settings.h"
#include <ui/common/ui_resource_name.h>

typedef QnBusinessActionData* QnLightBusinessActionP;

// -------------------------------------------------------------------------- //
// QnEventLogModel::DataIndex
// -------------------------------------------------------------------------- //
class QnAuditLogSessionModel::DataIndex
{
public:
    DataIndex(): m_sortCol(TimestampColumn), m_sortOrder(Qt::DescendingOrder)
    {
    }

    void setSort(int column, Qt::SortOrder order)
    { 
        if ((Column) column == m_sortCol) {
            m_sortOrder = order;
            return;
        }

        m_sortCol = (Column) column;
        m_sortOrder = order;

        updateIndex();
    }

    void setData(const QnAuditRecordList &data)
    { 
        m_data = data;
        updateIndex();
    }

    QnAuditRecordList data() const
    {
        return m_data;
    }

    void clear()
    {
        m_data.clear();
    }

    inline int size() const
    {
        return m_data.size();
    }


    inline QnAuditRecord& at(int row)
    {
        return m_sortOrder == Qt::AscendingOrder ? m_data[row] : m_data[m_data.size()-1-row];
    }

    // comparators

    typedef bool (*LessFunc)(const QnAuditRecord &d1, const QnAuditRecord &d2);

    static bool lessThanTimestamp(const QnAuditRecord &d1, const QnAuditRecord &d2)
    {
        return d1.timestamp < d2.timestamp;
    }

    static bool lessThanEndTimestamp(const QnAuditRecord &d1, const QnAuditRecord &d2)
    {
        return d1.endTimestamp < d2.endTimestamp;
    }

    static bool lessThanDuration(const QnAuditRecord &d1, const QnAuditRecord &d2)
    {
        return (d1.endTimestamp - d1.timestamp) < (d2.endTimestamp - d2.timestamp);
    }

    static bool lessThanUserName(const QnAuditRecord &d1, const QnAuditRecord &d2)
    {
        return d1.userName < d2.userName;
    }

    static bool lessThanUserHost(const QnAuditRecord &d1, const QnAuditRecord &d2)
    {
        return d1.userHost < d2.userHost;
    }

    void updateIndex()
    {
        LessFunc lessThan = &lessThanTimestamp;
        switch(m_sortCol) {
            case TimestampColumn:
                lessThan = &lessThanTimestamp;
                break;
            case EndTimestampColumn:
                lessThan = &lessThanEndTimestamp;
                break;
            case DurationColumn:
                lessThan = &lessThanDuration;
                break;
            case UserNameColumn:
                lessThan = &lessThanUserName;
                break;
            case UserHostColumn:
                lessThan = &lessThanUserHost;
                break;
        }

        qSort(m_data.begin(), m_data.end(), lessThan);
    }

private:
    Column m_sortCol;
    Qt::SortOrder m_sortOrder;
    QnAuditRecordList m_data;
};


// -------------------------------------------------------------------------- //
// QnEventLogModel
// -------------------------------------------------------------------------- //
QnAuditLogSessionModel::QnAuditLogSessionModel(QObject *parent):
    base_type(parent)
{
    m_index = new DataIndex();
}

QnAuditLogSessionModel::~QnAuditLogSessionModel() {
    delete m_index;
}

void QnAuditLogSessionModel::setData(const QnAuditRecordList &data) {
    beginResetModel();
    m_index->setData(data);
    endResetModel();
}

void QnAuditLogSessionModel::clear() {
    beginResetModel();
    m_index->clear();
    endResetModel();
}

QModelIndex QnAuditLogSessionModel::index(int row, int column, const QModelIndex &parent) const 
{
    return hasIndex(row, column, parent) 
        ? createIndex(row, column, (void*)0) 
        : QModelIndex();
}

QModelIndex QnAuditLogSessionModel::parent(const QModelIndex &) const {
    return QModelIndex();
}

QString QnAuditLogSessionModel::getResourceNameString(QnUuid id) {
    return getResourceName(qnResPool->getResourceById(id));
}

QString QnAuditLogSessionModel::formatDateTime(int timestampSecs) const
{
    return QDateTime::fromMSecsSinceEpoch(timestampSecs * 1000ll).toString(Qt::DefaultLocaleShortDate);
}

QString QnAuditLogSessionModel::formatDuration(int duration) const
{
    int seconds = duration % 60;
    duration /= 60;
    int minutes = duration % 60;
    duration /= 60;
    int hours = duration % 24;
    duration /= 24;
    int days = duration;
    
    QString result;
    if (days > 0)
        result += tr("%1d ").arg(days);
    if (hours > 0)
        result += tr("%1h ").arg(hours);
    if (minutes > 0 && days == 0)
        result += tr("%1m ").arg(minutes);
    //if (seconds > 0 && days == 0 && hours == 0)
    //    result += tr("%1s ").arg(seconds);
    return result;
}

QString QnAuditLogSessionModel::textData(const Column& column,const QnAuditRecord& data) const
{
    switch(column) {
    case TimestampColumn:
        return formatDateTime(data.timestamp);
    case EndTimestampColumn:
        if (data.eventType == Qn::AR_Login)
            return formatDateTime(data.endTimestamp);
        else if(data.eventType == Qn::AR_UnauthorizedLogin)
            return tr("Unsuccessful login");
        break;
    case DurationColumn:
        if (data.endTimestamp)
            return formatDuration(data.endTimestamp - data.timestamp);
        else
            return QString();
    case UserNameColumn:
        return data.userName;
    case UserHostColumn:
        return data.userHost;
        break;
    }

    return QString();
}

void QnAuditLogSessionModel::sort(int column, Qt::SortOrder order) {
    beginResetModel();
    m_index->setSort(column, order);
    endResetModel();
}

int QnAuditLogSessionModel::columnCount(const QModelIndex &parent) const {
    Q_UNUSED(parent);
    return ColumnCount;
}

int QnAuditLogSessionModel::rowCount(const QModelIndex &parent) const {
    Q_UNUSED(parent);
    return m_index->size(); // TODO: #Elric incorrect, should return zero for non-root nodes.
}

QVariant QnAuditLogSessionModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        switch(section) {
            case TimestampColumn:
                return tr("Session begins");
            case EndTimestampColumn:
                return tr("Session ends");
            case DurationColumn:
                return tr("Duration");
            case UserNameColumn:
                return tr("User");
            case UserHostColumn:
                return tr("IP");
            case UserActivityColumn:
                return tr("Activity");
        default:
            break;
        }
    }
    return base_type::headerData(section, orientation, role);
}

QVariant QnAuditLogSessionModel::data(const QModelIndex &index, int role) const 
{
    if (!index.isValid() || index.model() != this || !hasIndex(index.row(), index.column(), index.parent()))
        return QVariant();

    const QnAuditRecord &record = m_index->at(index.row());
    
    switch(role) {
    case Qt::DisplayRole:
        return QVariant(textData((Column) index.column(), record));
    default:
        break;
    }

    return QVariant();
}
