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
#include "ui/style/skin.h"

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
            case SelectRowColumn:
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
    if ((minutes > 0 && days == 0) || result.isEmpty())
        result += tr("%1m ").arg(minutes);
    //if (seconds > 0 && days == 0 && hours == 0)
    //    result += tr("%1s ").arg(seconds);
    return result;
}

QString QnAuditLogModel::eventTypeToString(Qn::AuditRecordType eventType) const
{
    switch (eventType)
    {
    case Qn::AR_NotDefined:
            return tr("Unknown");
        case Qn::AR_UnauthorizedLogin:
            return tr("Unsuccessful login");
        case Qn::AR_Login:
            return tr("Login");
        case Qn::AR_UserUpdate:
            return tr("User updated");
        case Qn::AR_ViewLive:
            return tr("Watching live");
        case Qn::AR_ViewArchive:
            return tr("Watching archive");
        case Qn::AR_ExportVideo:
            return tr("Exporting video");
        case Qn::AR_CameraUpdate:
            return tr("Camera updated");
        case Qn::AR_SystemNameChanged:
            return tr("System name changed");
        case Qn::AR_SystemmMerge:
            return tr("System merge");
        case Qn::AR_GeneralSettingsChange:
            return tr("General settings updated");
        case Qn::AR_ServerUpdate:
            return tr("Server updated");
        case Qn::AR_BEventUpdate:
            return tr("Business rule updated");
        case Qn::AR_EmailSettings:
            return tr("E-mail updated");
    }
    return QString();
}

QString QnAuditLogModel::buttonNameForEvent(Qn::AuditRecordType eventType) const
{
    switch (eventType)
    {
    case Qn::AR_ViewLive:
    case Qn::AR_ViewArchive:
    case Qn::AR_ExportVideo:
        return tr("Play this");
    case Qn::AR_UserUpdate:
    case Qn::AR_ServerUpdate:
    case Qn::AR_CameraUpdate:
        return tr("Settings");
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

QString QnAuditLogModel::htmlData(const Column& column,const QnAuditRecord& data, int row, bool hovered) const
{
    if (column == TimestampColumn)
    {
        QString dateStr = formatDateTime(data.createdTimeSec, true, false);
        QString timeStr = formatDateTime(data.createdTimeSec, false, true);
        return lit("%1 <b>%2</b>").arg(dateStr).arg(timeStr);
    }
    else if (column == DescriptionColumn) 
    {
        QString result;
        switch (data.eventType)
        {
        case Qn::AR_ViewArchive:
        case Qn::AR_ViewLive:
            result = tr("%1 - %2, ").arg(formatDateTime(data.rangeStartSec)).arg(formatDateTime(data.rangeEndSec));
        case Qn::AR_CameraUpdate:
        {
            QString txt = tr("%n cameras", "", data.resources.size());
            QString linkColor = lit("#%1").arg(QString::number(m_colors.httpLink.rgb(), 16));
            if (hovered)
                result +=  QString(lit("<font color=%1><u><b>%2</b></u></font>")).arg(linkColor).arg(txt);
            else
                result +=  QString(lit("<font color=%1><b>%2</b></font>")).arg(linkColor).arg(txt);
            bool showDetail = data.extractParam("detail") == "1";
            if (showDetail) 
            {
                auto archiveData = data.extractParam("archiveExist");
                int index = 0;
                for (const auto& camera: data.resources) 
                {
                    result += lit("<br> ");
                    if (data.isPlaybackType())
                    {
                        bool isRecordExist = archiveData.size() > index && archiveData[index] == '1';
                        index++;
                        QChar circleSymbol = isRecordExist ? QChar(0x25CF) : QChar(0x25CB);
                        if (isRecordExist)
                            result += QString(lit("<font size=5 color=red>%1</font>")).arg(circleSymbol);
                        else
                            result += QString(lit("<font size=5>%1</font>")).arg(circleSymbol);
                    }
                    result += getResourceNameString(camera);
                }
            }
            return result;
        }
        }
    }
    
    return textData(column, data, row);
}


QString QnAuditLogModel::textData(const Column& column,const QnAuditRecord& data, int row) const
{
    switch(column) {
    case SelectRowColumn:
        return QString();
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
            return eventTypeToString(data.eventType);
        break;
    case DurationColumn:
        if (data.rangeEndSec)
            return formatDuration(data.rangeEndSec - data.rangeStartSec);
        else
            return formatDuration(0);
    case UserNameColumn:
        return data.userName;
    case UserHostColumn:
        return data.userHost;
        break;
    case EventTypeColumn:
        return eventTypeToString(data.eventType);
    case DescriptionColumn:
        return eventDescriptionText(data);
    case PlayButtonColumn:
        return buttonNameForEvent(data.eventType);
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
            case SelectRowColumn:
                return QVariant();
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

Qt::CheckState QnAuditLogModel::checkState() const
{
    bool onExist = false;
    bool offExist = false;

    if (m_index->size() == 0)
        return Qt::Unchecked;

    for (int i = 0; i < m_index->size(); ++i)
    {
        if (m_index->at(i).extractParam("checked") ==  "1")
            onExist = true;
        else
            offExist = true;
    }
    if (onExist && offExist)
        return Qt::PartiallyChecked;
    else if (onExist)
        return Qt::Checked;
    else
        return Qt::Unchecked;
}

void QnAuditLogModel::setCheckState(Qt::CheckState state)
{
    if (state == Qt::Checked) {
        for (int i = 0; i < m_index->size(); ++i)
        {
            m_index->at(i).addParam("checked", "1");
        }
    } 
    else if (state == Qt::Unchecked) {
        for (int i = 0; i < m_index->size(); ++i)
        {
            m_index->at(i).removeParam("checked");
        }
    }
    else {
        return; // partial checked
    }
    emit dataChanged(index(0,0), index(m_index->size(), m_columns.size()), QVector<int>() << Qt::CheckStateRole);
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
        emit dataChanged(index, index, QVector<int>() << role);
        return true;
    }
    else if (role == Qt::CheckStateRole)
    {
        if (value == Qt::Checked)
            m_index->at(index.row()).addParam("checked", "1");
        else
            m_index->at(index.row()).removeParam("checked");
        emit dataChanged(index, index, QVector<int>() << role);
        return true;
    }
    else {
        return base_type::setData(index, value, role);
    }
}

QnAuditRecordList QnAuditLogModel::checkedRows()
{
    QnAuditRecordList result;
    for (const auto& record: m_index->data())
    {
        if (record.extractParam("checked") == "1")
            result.push_back(record);
    }
    return result;
}

QnAuditRecord QnAuditLogModel::rawData(int row) const
{
    return m_index->at(row);
}

QVariant QnAuditLogModel::colorForType(Qn::AuditRecordType actionType) const
{
    switch (actionType)
    {
    case Qn::AR_UnauthorizedLogin:
    case Qn::AR_Login:
        return m_colors.loginAction;
    case Qn::AR_UserUpdate:
        return m_colors.updUsers;
    case Qn::AR_ViewLive:
        return m_colors.watchingLive;
    case Qn::AR_ViewArchive:
        return m_colors.watchingArchive;
    case Qn::AR_ExportVideo:
        return m_colors.exportVideo;
    case Qn::AR_CameraUpdate:
        return m_colors.updCamera;
    case Qn::AR_SystemNameChanged:
    case Qn::AR_SystemmMerge:
    case Qn::AR_GeneralSettingsChange:
        return m_colors.systemActions;
    case Qn::AR_ServerUpdate:
        return m_colors.updServer;
    case Qn::AR_BEventUpdate:
        return m_colors.eventRules;
    case Qn::AR_EmailSettings:
        return m_colors.emailSettings;
    default:
        return QVariant();
    }
}

QVariant QnAuditLogModel::data(const QModelIndex &index, int role) const 
{
    if (!index.isValid() || index.model() != this || !hasIndex(index.row(), index.column(), index.parent()))
        return QVariant();

    const Column &column = m_columns[index.column()];


    const QnAuditRecord &record = m_index->at(index.row());
    
    switch(role) {
    case Qt::CheckStateRole:
        if (column == SelectRowColumn)
            return record.extractParam("checked") == "1" ? Qt::Checked : Qt::Unchecked;
        else
            return QVariant();
    case Qt::DisplayRole:
        return QVariant(textData(column, record, index.row()));
    case Qn::AuditRecordDataRole:
        return QVariant::fromValue<QnAuditRecord>(m_index->at(index.row()));
    case Qn::DisplayHtmlRole:
        return htmlData(column, record, index.row(), false);
    case Qn::DisplayHtmlHoveredRole:
        return htmlData(column, record, index.row(), true);
    case Qn::ColumnDataRole:
        return column;
    case Qt::ForegroundRole:
        if (column == EventTypeColumn)
            return colorForType(record.eventType);
        else
            return QVariant();
    case Qt::FontRole:
        if (column == DateColumn) {
            QFont font;
            font.setBold(true);
            return font;
        }
        else
            return QVariant();
    case Qt::DecorationRole:
    case Qn::DecorationHoveredRole:
    {
        if (column != PlayButtonColumn)
            return QVariant();
        if (record.isPlaybackType()) {
            if (role == Qt::DecorationRole)
                return qnSkin->icon("slider/navigation/play.png");
            else
                return qnSkin->icon("slider/navigation/play_hovered.png");
        }
        else if (record.eventType == Qn::AR_UserUpdate)
            return qnSkin->icon("tree/user.png");
        else if (record.eventType == Qn::AR_ServerUpdate)
            return qnSkin->icon("tree/server.png");
        else if (record.eventType == Qn::AR_CameraUpdate)
            return qnSkin->icon("tree/camera.png");
    }
    default:
        break;
    }

    return QVariant();
}

void QnAuditLogModel::setColumns(const QList<Column> &columns)
{
    m_columns = columns;
}

QnAuditLogColors QnAuditLogModel::colors() const
{
    return m_colors;
}

void QnAuditLogModel::setColors(const QnAuditLogColors &colors)
{
    m_colors = colors;
}
