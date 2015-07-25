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
class QnAuditLogModel::DataIndex
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
        return d1.createdTimeSec < d2.createdTimeSec;
    }

    static bool lessThanEndTimestamp(const QnAuditRecord &d1, const QnAuditRecord &d2)
    {
        return d1.rangeEndSec < d2.rangeEndSec;
    }

    static bool lessThanDuration(const QnAuditRecord &d1, const QnAuditRecord &d2)
    {
        return (d1.rangeEndSec - d1.rangeStartSec) < (d2.rangeEndSec - d2.rangeStartSec);
    }

    static bool lessThanUserName(const QnAuditRecord &d1, const QnAuditRecord &d2)
    {
        return d1.userName < d2.userName;
    }

    static bool lessThanUserHost(const QnAuditRecord &d1, const QnAuditRecord &d2)
    {
        return d1.userHost < d2.userHost;
    }

    static bool lessThanEventType(const QnAuditRecord &d1, const QnAuditRecord &d2)
    {
        return d1.eventType < d2.eventType;
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
            case EventTypeColumn:
                lessThan = &lessThanEventType;
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
QnAuditLogModel::QnAuditLogModel(QObject *parent):
    base_type(parent)
{
    m_index = new DataIndex();
}

QnAuditLogModel::~QnAuditLogModel() {
    delete m_index;
}

void QnAuditLogModel::setData(const QnAuditRecordList &data) {
    beginResetModel();
    m_index->setData(data);
    endResetModel();
}

void QnAuditLogModel::clear() {
    beginResetModel();
    m_index->clear();
    endResetModel();
}

QModelIndex QnAuditLogModel::index(int row, int column, const QModelIndex &parent) const 
{
    return hasIndex(row, column, parent) 
        ? createIndex(row, column, (void*)0) 
        : QModelIndex();
}

QModelIndex QnAuditLogModel::parent(const QModelIndex &) const {
    return QModelIndex();
}

QString QnAuditLogModel::getResourceNameString(QnUuid id) const {
    return getResourceName(qnResPool->getResourceById(id));
}

QString QnAuditLogModel::formatDateTime(int timestampSecs, bool showDate, bool showTime) const
{
    if (timestampSecs == 0)
        return QString();
    QDateTime dateTime = QDateTime::fromMSecsSinceEpoch(timestampSecs * 1000ll);
    if (showDate && showTime)
        return dateTime.toString(Qt::DefaultLocaleShortDate);
    else if (showDate)
        return dateTime.date().toString(Qt::DefaultLocaleShortDate);
    else if (showTime)
        return dateTime.time().toString(Qt::DefaultLocaleShortDate);
    else
        return QString();
}

QString QnAuditLogModel::formatDuration(int duration) const
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

QString QnAuditLogModel::eventTypeToString(Qn::AuditRecordType recordType) const
{
    switch (recordType)
    {
    case Qn::AR_NotDefined:
            return tr("Unknown");
        case Qn::AR_UnauthorizedLogin:
            return tr("Unsuccessful login");
        case Qn::AR_Login:
            return tr("Login");
        case Qn::AR_SystemNameChanged:
            return tr("System name changed");
        case Qn::AR_SystemmMerge:
            return tr("Merge systems");
        case Qn::AR_CameraUpdate:
            return tr("Camera(s) updated");
        case Qn::AR_ServerUpdate:
            return tr("Server updated");
        case Qn::AR_GeneralSettingsChange:
            return tr("General settings changed");
        case Qn::AR_ViewArchive:
            return tr("Watching archive");
        case Qn::AR_ViewLive:
            return tr("Watching live");
    }
    return QString();
}

QString QnAuditLogModel::eventDescriptionText(const QnAuditRecord& data) const
{
    QString resListText;
    for (const auto& res: data.resources)
    {
        if (!resListText.isEmpty())
            resListText += lit(",");
        resListText += getResourceNameString(res);
    }

    switch (data.eventType)
    {
    case Qn::AR_ViewArchive:
    case Qn::AR_ViewLive:
    case Qn::AR_CameraUpdate:
        return tr("Cameras: ") + resListText;
    case Qn::AR_ServerUpdate:
        return tr("Servers:") + resListText;
    }
    return QString();
}

QString QnAuditLogModel::htmlData(const Column& column,const QnAuditRecord& data, int row) const
{
    if (column != DescriptionColumn)
        return textData(column, data, row);
    QString result;
    switch (data.eventType)
    {
    case Qn::AR_ViewArchive:
    case Qn::AR_ViewLive:
        result = tr("%1 - %2, ").arg(formatDateTime(data.rangeStartSec)).arg(formatDateTime(data.rangeEndSec));
    case Qn::AR_CameraUpdate:
    {
        QString txt = tr("%n cameras", "", data.resources.size());
        result +=  QString(lit("<a href=\"cameras\">%1</a><br>")).arg(txt);
        bool showDetail = data.extractParam("detail") == "1";
        if (showDetail) 
        {
            auto archiveData = data.extractParam("archiveExist");
            int index = 0;
            for (const auto& camera: data.resources) 
            {
                bool isRecordExist = archiveData.size() > index && archiveData[index] == '1';
                index++;
                QChar circleSymbol = isRecordExist ? QChar(0x25CF) : QChar(0x25CB);
                result += QString(lit("<br> <font color=red>%1</font> %2")).arg(circleSymbol).arg(getResourceNameString(camera));
            }
        }
        return result;
    }
    }
    
    return textData(column, data, row);
}


QString QnAuditLogModel::textData(const Column& column,const QnAuditRecord& data, int row) const
{
    switch(column) {
    case TimestampColumn:
        return formatDateTime(data.createdTimeSec, true, true);
    case DateColumn:
        if (row > 0) {
            QDate d1 = QDateTime::fromMSecsSinceEpoch(data.createdTimeSec*1000).date();
            QDate d2 = QDateTime::fromMSecsSinceEpoch(m_index->at(row-1).createdTimeSec*1000).date();
            if (d1 == d2)
                return QString();
        }
        return formatDateTime(data.createdTimeSec, true, false);
    case TimeColumn:
        return formatDateTime(data.createdTimeSec, false, true);
    case EndTimestampColumn:
        if (data.eventType == Qn::AR_Login)
            return formatDateTime(data.rangeEndSec, true, true);
        else if(data.eventType == Qn::AR_UnauthorizedLogin)
            return tr("Unsuccessful login");
        break;
    case DurationColumn:
        if (data.rangeEndSec)
            return formatDuration(data.rangeEndSec - data.rangeStartSec);
        else
            return QString();
    case UserNameColumn:
        return data.userName;
    case UserHostColumn:
        return data.userHost;
        break;
    case EventTypeColumn:
        return eventTypeToString(data.eventType);
    case DescriptionColumn:
        return eventDescriptionText(data);
    }

    return QString();
}

void QnAuditLogModel::sort(int column, Qt::SortOrder order) {
    beginResetModel();
    m_index->setSort(column, order);
    endResetModel();
}

int QnAuditLogModel::rowCount(const QModelIndex &parent) const {
    Q_UNUSED(parent);
    return m_index->size(); // TODO: #Elric incorrect, should return zero for non-root nodes.
}

QVariant QnAuditLogModel::headerData(int section, Qt::Orientation orientation, int role) const 
{
    if (section >= m_columns.size())
        return QVariant();

    const Column &column = m_columns[section];

    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        switch(column) {
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
            case EventTypeColumn:
                return tr("Activity");
            case DateColumn:
                return tr("Date");
            case TimeColumn:
                return tr("Time");
            case DescriptionColumn:
                return tr("Description");
            case PlayButtonColumn:
                return tr("View it");
        }
    }
    return base_type::headerData(section, orientation, role);
}

bool QnAuditLogModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || index.model() != this || !hasIndex(index.row(), index.column(), index.parent()))
        return false;

    if (role == Qn::AuditRecordDataRole)
    {
        if (!value.canConvert<QnAuditRecord>())
            return false;
        QnAuditRecord record = value.value<QnAuditRecord>();
        m_index->at(index.row()) = record;
        return true;
    }
    else {
        return base_type::setData(index, value, role);
    }
}


QVariant QnAuditLogModel::data(const QModelIndex &index, int role) const 
{
    if (!index.isValid() || index.model() != this || !hasIndex(index.row(), index.column(), index.parent()))
        return QVariant();

    const Column &column = m_columns[index.column()];


    const QnAuditRecord &record = m_index->at(index.row());
    
    switch(role) {
    case Qt::DisplayRole:
        return QVariant(textData(column, record, index.row()));
    case Qn::AuditRecordDataRole:
        return QVariant::fromValue<QnAuditRecord>(m_index->at(index.row()));
    case Qn::DisplayHtmlRole:
        return htmlData(column, record, index.row());
    case Qn::ColumnDataRole:
        return column;
    //case Qt::SizeHintRole:
    //    return QSize(64, 32);
    default:
        break;
    }

    return QVariant();
}

void QnAuditLogModel::setColumns(const QList<Column> &columns)
{
    m_columns = columns;
}
