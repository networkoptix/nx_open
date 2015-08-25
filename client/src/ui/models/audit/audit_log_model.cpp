#include "audit_log_model.h"

#include <cassert>

#include <QtGui/QPalette>

#include <core/resource/resource.h>
#include <core/resource/network_resource.h>

#include "core/resource_management/resource_pool.h"
#include "ui/style/resource_icon_cache.h"
#include "client/client_globals.h"
#include <utils/math/math.h>
#include "client/client_settings.h"
#include <ui/common/ui_resource_name.h>
#include "ui/style/skin.h"
#include "api/common_message_processor.h"
#include "business/business_strings_helper.h"

#include <utils/common/warnings.h>
#include <utils/common/synctime.h>

typedef QnBusinessActionData* QnLightBusinessActionP;

const QByteArray QnAuditLogModel::ChildCntParamName("childCnt");
const QByteArray QnAuditLogModel::CheckedParamName("checked");

namespace
{
    QString firstResourceName(const QnAuditRecord *d1)
    {
        if (d1->resources.empty())
            return QString();
        if (QnResourcePtr res = qnResPool->getResourceById(d1->resources[0]))
            return res->getName();
        else
            return QString();
    }

    QString firstResourceIp(const QnAuditRecord *d1)
    {
        if (d1->resources.empty())
            return QString();
        if (QnNetworkResourcePtr res = qnResPool->getResourceById<QnNetworkResource>(d1->resources[0]))
            return res->getHostAddress();
        else
            return QString();
    }
}

// -------------------------------------------------------------------------- //
// QnEventLogModel::DataIndex
// -------------------------------------------------------------------------- //
class QnAuditLogModel::DataIndex
{
public:
    DataIndex(): m_sortCol(TimestampColumn), m_sortOrder(Qt::DescendingOrder)
    {
    }

    void setSort(Column column, Qt::SortOrder order)
    { 
        if ((Column) column == m_sortCol) {
            m_sortOrder = order;
            return;
        }
        m_sortCol = column;
        m_sortOrder = order;

        updateIndex();
    }

    void setData(const QnAuditRecordRefList &data)
    { 
        m_data = data;
        updateIndex();
    }

    QnAuditRecordRefList data() const
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


    inline QnAuditRecord* at(int row)
    {
        return m_sortOrder == Qt::AscendingOrder ? m_data[row] : m_data[m_data.size()-1-row];
    }

    // comparators

    typedef bool (*LessFunc)(const QnAuditRecord *d1, const QnAuditRecord *d2);

    static bool lessThanTimestamp(const QnAuditRecord *d1, const QnAuditRecord *d2)
    {
        return d1->createdTimeSec < d2->createdTimeSec;
    }

    static bool lessThanEndTimestamp(const QnAuditRecord *d1, const QnAuditRecord *d2)
    {
        return d1->rangeEndSec < d2->rangeEndSec;
    }

    static bool lessThanDuration(const QnAuditRecord *d1, const QnAuditRecord *d2)
    {
        return (d1->rangeEndSec - d1->rangeStartSec) < (d2->rangeEndSec - d2->rangeStartSec);
    }

    static bool lessThanUserName(const QnAuditRecord *d1, const QnAuditRecord *d2)
    {
        return d1->authSession.userName < d2->authSession.userName;
    }

    static bool lessThanUserHost(const QnAuditRecord *d1, const QnAuditRecord *d2)
    {
        return d1->authSession.userHost < d2->authSession.userHost;
    }

    static bool lessThanEventType(const QnAuditRecord *d1, const QnAuditRecord *d2)
    {
        return d1->eventType < d2->eventType;
    }

    static bool lessThanCameraName(const QnAuditRecord *d1, const QnAuditRecord *d2)
    {
        return firstResourceName(d1) < firstResourceName(d2);
    }

    static bool lessThanCameraIp(const QnAuditRecord *d1, const QnAuditRecord *d2)
    {
        return firstResourceIp(d1) < firstResourceIp(d2);
    }

    static bool lessThanActivity(const QnAuditRecord *d1, const QnAuditRecord *d2)
    {
        return d1->extractParam(ChildCntParamName).toUInt() < d2->extractParam("childCnt").toUInt();
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

            case CameraNameColumn:
                lessThan = &lessThanCameraName;
                break;
            case CameraIpColumn:
                lessThan = &lessThanCameraIp;
                break;

            case UserActivityColumn:
                lessThan = &lessThanActivity;
                break;
        }

        qSort(m_data.begin(), m_data.end(), lessThan);
    }

private:
    Column m_sortCol;
    Qt::SortOrder m_sortOrder;
    QnAuditRecordRefList m_data;
};


// -------------------------------------------------------------------------- //
// QnEventLogModel
// -------------------------------------------------------------------------- //

bool QnAuditLogModel::hasDetail(const QnAuditRecord* record)
{
    return record->extractParam("detail") == "1";
}

void QnAuditLogModel::setDetail(QnAuditRecord* record, bool showDetail)
{
    record->addParam("detail", showDetail ? "1" : "0");
}

QnAuditLogModel::QnAuditLogModel(QObject *parent):
    base_type(parent)
    , m_index(new DataIndex())
{
    
}

QnAuditLogModel::~QnAuditLogModel() {
}

void QnAuditLogModel::setData(const QnAuditRecordRefList &data) {
    beginResetModel();
    m_index->setData(data);
    endResetModel();
}

void QnAuditLogModel::clearData()
{
    beginResetModel();
    m_index->clear();
    m_interleaveInfo.clear();
    endResetModel();
}

void QnAuditLogModel::clear() {
    beginResetModel();
    m_index->clear();
    endResetModel();
}

QString QnAuditLogModel::getResourceNameById(const QnUuid &id) 
{
    return getResourceName(qnResPool->getResourceById(id));
}

QString QnAuditLogModel::formatDateTime(int timestampSecs, bool showDate, bool showTime)
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

QString QnAuditLogModel::formatDuration(int durationSecs)
{
    int duration = durationSecs;
    /* int seconds = duration % 60; */
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

QString QnAuditLogModel::eventTypeToString(Qn::AuditRecordType eventType)
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
        case Qn::AR_CameraInsert:
            return tr("Camera added");
        case Qn::AR_SystemNameChanged:
            return tr("System name changed");
        case Qn::AR_SystemmMerge:
            return tr("System merge");
        case Qn::AR_SettingsChange:
            return tr("General settings updated");
        case Qn::AR_ServerUpdate:
            return tr("Server updated");
        case Qn::AR_BEventUpdate:
            return tr("Business rule updated");
        case Qn::AR_EmailSettings:
            return tr("E-mail settings changed");
        case Qn::AR_CameraRemove:
            return tr("Camera removed");
        case Qn::AR_ServerRemove:
            return tr("Server removed");
        case Qn::AR_BEventRemove:
            return tr("Business rule removed");
        case Qn::AR_UserRemove:
            return tr("User removed");
        case Qn::AR_BEventReset:
            return tr("Business rule reseted");
        case Qn::AR_DatabaseRestore:
            return tr("Database restored");
    }
    return QString();
}

QString QnAuditLogModel::buttonNameForEvent(Qn::AuditRecordType eventType)
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
    case Qn::AR_CameraInsert:
        return tr("Settings");
    }
    return QString();
}

QString QnAuditLogModel::getResourcesString(const std::vector<QnUuid>& resources)
{
    QString result;
    for (const auto& res: resources)
    {
        if (!result.isEmpty())
            result += lit(",");
        result += getResourceNameById(res);
    }
    return result;
}

QString QnAuditLogModel::eventDescriptionText(const QnAuditRecord* data)
{
    QString result;
    switch (data->eventType)
    {
    case Qn::AR_CameraRemove:
    case Qn::AR_ServerRemove:
    case Qn::AR_BEventRemove:
    case Qn::AR_BEventUpdate:
    case Qn::AR_UserRemove:
    case Qn::AR_SystemNameChanged:
        result = QString::fromUtf8(data->extractParam("description"));
        break;
    case Qn::AR_UnauthorizedLogin:
    case Qn::AR_Login:
        result = data->authSession.userAgent;
        break;
    case Qn::AR_ViewArchive:
    case Qn::AR_ViewLive:
    case Qn::AR_ExportVideo:
        result = tr("%1 - %2, ").arg(formatDateTime(data->rangeStartSec)).arg(formatDateTime(data->rangeEndSec));
    case Qn::AR_CameraUpdate:
    case Qn::AR_CameraInsert:
        result +=  tr("%n cameras", "", static_cast<int>(data->resources.size()));
        break;
    default:
        result = getResourcesString(data->resources);
    }
    return result;
}

QString QnAuditLogModel::htmlData(const Column& column,const QnAuditRecord* data, int row, bool hovered) const
{
    if (column == TimestampColumn)
    {
        QString dateStr = formatDateTime(data->createdTimeSec, true, false);
        QString timeStr = formatDateTime(data->createdTimeSec, false, true);
        return lit("%1 <b>%2</b>").arg(dateStr).arg(timeStr);
    }
    else if (column == DescriptionColumn) 
    {
        QString result;
        switch (data->eventType)
        {
        case Qn::AR_ViewArchive:
        case Qn::AR_ViewLive:
        case Qn::AR_ExportVideo:
            result = tr("%1 - %2, ").arg(formatDateTime(data->rangeStartSec)).arg(formatDateTime(data->rangeEndSec));
        case Qn::AR_CameraInsert:
        case Qn::AR_CameraUpdate:
        {
            QString txt = tr("%n cameras", "", static_cast<int>(data->resources.size()));
            QString linkColor = lit("#%1").arg(QString::number(m_colors.httpLink.rgb(), 16));
            if (hovered)
                result +=  QString(lit("<font color=%1><u><b>%2</b></u></font>")).arg(linkColor).arg(txt);
            else
                result +=  QString(lit("<font color=%1><b>%2</b></font>")).arg(linkColor).arg(txt);
            if (hasDetail(data)) 
            {
                auto archiveData = data->extractParam("archiveExist");
                int index = 0;
                for (const auto& camera: data->resources) 
                {
                    result += lit("<br> ");
                    if (data->isPlaybackType())
                    {
                        bool isRecordExist = archiveData.size() > index && archiveData[index] == '1';
                        index++;
                        QChar circleSymbol = isRecordExist ? QChar(0x25CF) : QChar(0x25CB);
                        if (isRecordExist)
                            result += QString(lit("<font size=5 color=red>%1</font>")).arg(circleSymbol);
                        else
                            result += QString(lit("<font size=5>%1</font>")).arg(circleSymbol);
                    }
                    result += getResourceNameById(camera);
                }
            }
            return result;
        }
        }
    }
    
    if (column == DateColumn && skipDate(data, row))
        return QString();
    else
        return textData(column, data);
}


QString QnAuditLogModel::makeSearchPattern(const QnAuditRecord* record)
{
    Column columnsToFilter[] = 
    {
        TimestampColumn,
        EndTimestampColumn,
        DurationColumn,
        UserNameColumn,
        UserHostColumn,
        EventTypeColumn,
        DescriptionColumn,
        PlayButtonColumn
    };
    QString result;
    for(const auto& column: columnsToFilter)
        result += searchData(column, record);
    return result;
}

QString QnAuditLogModel::searchData(const Column& column, const QnAuditRecord* data)
{
    QString result = textData(column, data);
    if (column == DescriptionColumn && (data->isPlaybackType() || data->eventType == Qn::AR_CameraUpdate || data->eventType == Qn::AR_CameraInsert))
        result += getResourcesString(data->resources); // text description doesn't contain resources for that types, but their names need for search
    return result;
}


QString QnAuditLogModel::textData(const Column& column,const QnAuditRecord* data)
{
    switch(column) {
    case SelectRowColumn:
        return QString();
    case TimestampColumn:
        return formatDateTime(data->createdTimeSec, true, true);
    case DateColumn:
        return formatDateTime(data->createdTimeSec, true, false);
    case TimeColumn:
        return formatDateTime(data->createdTimeSec, false, true);
    case EndTimestampColumn:
        if (data->eventType == Qn::AR_Login)
            return data->rangeEndSec ? formatDateTime(data->rangeEndSec, true, true) : QString();
        else if(data->eventType == Qn::AR_UnauthorizedLogin)
            return eventTypeToString(data->eventType);
        break;
    case DurationColumn:
        if (data->rangeEndSec)
            return formatDuration(data->rangeEndSec - data->rangeStartSec);
        else
            return formatDuration(0);
    case UserNameColumn:
        return data->authSession.userName;
    case UserHostColumn:
        return data->authSession.userHost;
        break;
    case EventTypeColumn:
        return eventTypeToString(data->eventType);
    case CameraNameColumn:
        return firstResourceName(data);
    case CameraIpColumn:
        return firstResourceIp(data);
    case DescriptionColumn:
        return eventDescriptionText(data);
    case PlayButtonColumn:
        return buttonNameForEvent(data->eventType);
    case UserActivityColumn:
        return tr("%n action(s)", "", data->extractParam(ChildCntParamName).toUInt());
    }

    return QString();
}

void QnAuditLogModel::sort(int column, Qt::SortOrder order) {
    beginResetModel();
    m_index->setSort(m_columns[column], order);
    endResetModel();
}

int QnAuditLogModel::rowCount(const QModelIndex &parent) const {
    if(!parent.isValid())
        return m_index->size();
    return 0;
}

int QnAuditLogModel::columnCount(const QModelIndex &parent /* = QModelIndex()*/) const {
    if(!parent.isValid())
        return m_columns.size();
    return 0;
}

int QnAuditLogModel::minWidthForColumn(const Column &column) const
{
    switch(column) {
    case SelectRowColumn:
        return 8;
    case TimestampColumn:
    case EndTimestampColumn:
    case DurationColumn:
        return 80;
    case UserNameColumn:
    case UserHostColumn:
        return 48;
    case UserActivityColumn:
        return 80;
    case EventTypeColumn:
    case DateColumn:
    case TimeColumn:
        return 64;
    case DescriptionColumn:
        return 128;
    case PlayButtonColumn:
        return 64;
    case CameraNameColumn:
        return 200;
    default:
        return 64;
    }
}

QVariant QnAuditLogModel::headerData(int section, Qt::Orientation orientation, int role) const 
{
    if (section >= m_columns.size())
        return QVariant();

    const Column &column = m_columns[section];
    if (orientation != Qt::Horizontal)
        return base_type::headerData(section, orientation, role);
    
    if (role == Qt::SizeHintRole)
    {
        return QSize(minWidthForColumn(column), m_headerHeight);
    }
    else if (role == Qt::DisplayRole) 
    {
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
            case CameraNameColumn:
                return tr("Camera name");
            case CameraIpColumn:
                return tr("IP");
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
        if (m_index->at(i)->extractParam(CheckedParamName) ==  "1")
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
            m_index->at(i)->addParam(CheckedParamName, "1");
        }
    } 
    else if (state == Qt::Unchecked) {
        for (int i = 0; i < m_index->size(); ++i)
        {
            m_index->at(i)->removeParam(CheckedParamName);
        }
    }
    else {
        return; // partial checked
    }
    emit dataChanged(index(0,0), index(m_index->size(), m_columns.size()), QVector<int>() << Qt::CheckStateRole);
}

void QnAuditLogModel::setData(const QModelIndexList &indexList, const QVariant &value, int role)
{
    blockSignals(true);
    for (const auto& index: indexList)
        setData(index, value, role);
    blockSignals(false);
    emit dataChanged(index(0,0), index(m_index->size(), m_columns.size()), QVector<int>() << role);
}

bool QnAuditLogModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || index.model() != this || !hasIndex(index.row(), index.column(), index.parent()))
        return false;
    
    if (role == Qt::CheckStateRole)
    {
        if (value == Qt::Checked)
            m_index->at(index.row())->addParam(CheckedParamName, "1");
        else
            m_index->at(index.row())->removeParam(CheckedParamName);
        emit dataChanged(index, index, QVector<int>() << role);
        return true;
    }
    else {
        return base_type::setData(index, value, role);
    }
}

QnAuditRecordRefList QnAuditLogModel::checkedRows()
{
    QnAuditRecordRefList result;
    for (const auto& record: m_index->data())
    {
        if (record->extractParam(CheckedParamName) == "1")
            result.push_back(record);
    }
    return result;
}

QnAuditRecord* QnAuditLogModel::rawData(int row) const
{
    return m_index->at(row);
}

QVariant QnAuditLogModel::colorForType(Qn::AuditRecordType actionType) const
{
    switch (actionType)
    {
    case Qn::AR_UnauthorizedLogin:
        return m_colors.unsucessLoginAction;
    case Qn::AR_Login:
        return m_colors.loginAction;
    case Qn::AR_UserUpdate:
    case Qn::AR_UserRemove:
        return m_colors.updUsers;
    case Qn::AR_ViewLive:
        return m_colors.watchingLive;
    case Qn::AR_ViewArchive:
        return m_colors.watchingArchive;
    case Qn::AR_ExportVideo:
        return m_colors.exportVideo;
    case Qn::AR_CameraUpdate:
    case Qn::AR_CameraRemove:
    case Qn::AR_CameraInsert:
        return m_colors.updCamera;
    case Qn::AR_SystemNameChanged:
    case Qn::AR_SystemmMerge:
    case Qn::AR_SettingsChange:
    case Qn::AR_DatabaseRestore:
        return m_colors.systemActions;
    case Qn::AR_ServerUpdate:
    case Qn::AR_ServerRemove:
        return m_colors.updServer;
    case Qn::AR_BEventReset:
    case Qn::AR_BEventUpdate:
    case Qn::AR_BEventRemove:
        return m_colors.eventRules;
    case Qn::AR_EmailSettings:
        return m_colors.emailSettings;
    default:
        return QVariant();
    }
}

bool QnAuditLogModel::skipDate(const QnAuditRecord *record, int row) const
{
    if (row < 1) 
        return false;

    QDate d1 = QDateTime::fromMSecsSinceEpoch(record->createdTimeSec*1000).date();
    QDate d2 = QDateTime::fromMSecsSinceEpoch(m_index->at(row-1)->createdTimeSec*1000).date();
    return d1 == d2;
}

QString QnAuditLogModel::descriptionTooltip(const QnAuditRecord *record) const
{
    if (record->resources.empty())
        return QString();

    QString result;
    if (!hasDetail(record))
        result = tr("Click to expand");
    if (!record->isPlaybackType())
        return result;
    if (!result.isEmpty())
        result += lit("\n");
    result += tr("Red mark means that an archive is still available");
    return result;
}

QVariant QnAuditLogModel::data(const QModelIndex &index, int role) const 
{
    if (!index.isValid() || index.model() != this || !hasIndex(index.row(), index.column(), index.parent()))
        return QVariant();

    const Column &column = m_columns[index.column()];


    const QnAuditRecord *record = m_index->at(index.row());
    
    switch(role) {
    case Qt::CheckStateRole:
        if (column == SelectRowColumn)
            return record->extractParam(CheckedParamName) == "1" ? Qt::Checked : Qt::Unchecked;
        else
            return QVariant();
    case Qt::DisplayRole:
    {
        if (column == DateColumn && skipDate(record, index.row()))
            return QString();
        else
            return QVariant(textData(column, record));
    }
    case Qn::AuditRecordDataRole:
        return QVariant::fromValue<QnAuditRecord*>(m_index->at(index.row()));
    case Qn::DisplayHtmlRole:
        return htmlData(column, record, index.row(), false);
    case Qn::DisplayHtmlHoveredRole:
        return htmlData(column, record, index.row(), true);
    case Qn::ColumnDataRole:
        return column;
    case Qn::AlternateColorRole:
        return m_interleaveInfo.size() == m_index->size() && m_interleaveInfo[index.row()];
    case Qt::ForegroundRole:
        if (column == EventTypeColumn)
            return colorForType(record->eventType);
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
        if (record->isPlaybackType()) {
            if (role == Qt::DecorationRole)
                return qnSkin->icon("audit/play.png");
            else
                return qnSkin->icon("audit/play.png");
        }
        else if (record->eventType == Qn::AR_UserUpdate)
            return qnSkin->icon("tree/user.png");
        else if (record->eventType == Qn::AR_ServerUpdate)
            return qnSkin->icon("tree/server.png");
        else if (record->eventType == Qn::AR_CameraUpdate || record->eventType == Qn::AR_CameraInsert)
            return qnSkin->icon("tree/camera.png");
    }
    case Qt::ToolTipRole:
        if (column == DescriptionColumn)
            return descriptionTooltip(record);
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
    emit colorsChanged();
}

void QnAuditLogModel::calcColorInterleaving()
{
    QnUuid prevSessionId;
    int colorIndex = 0;
    m_interleaveInfo.resize(m_index->size());
    for (int i = 0; i < m_index->size(); ++i)
    {
        QnAuditRecord* record = m_index->at(i);
        if (record->authSession.id != prevSessionId) {
            colorIndex = (colorIndex + 1) % 2;
            prevSessionId = record->authSession.id;
        }
        m_interleaveInfo[i] = colorIndex;
    }
}

void QnAuditLogModel::setHeaderHeight(int value) {
    m_headerHeight = value;
}
