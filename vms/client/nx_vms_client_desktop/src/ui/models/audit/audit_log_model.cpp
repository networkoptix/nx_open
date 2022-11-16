// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "audit_log_model.h"

#include <cassert>

#include <QtGui/QPalette>

#include <api/common_message_processor.h>
#include <client/client_settings.h>
#include <client_core/client_core_module.h>
#include <common/common_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource/network_resource.h>
#include <core/resource/resource.h>
#include <core/resource/resource_display_info.h>
#include <core/resource_management/resource_pool.h>
#include <nx/branding.h>
#include <nx/vms/client/core/watchers/server_time_watcher.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/resource/search_helper.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <nx/vms/client/desktop/style/skin.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>
#include <nx/vms/common/html/html.h>
#include <nx/vms/event/strings_helper.h>
#include <nx/vms/text/human_readable.h>
#include <nx/vms/text/time_strings.h>
#include <nx/vms/time/formatter.h>
#include <ui/workbench/workbench_context.h>
#include <utils/common/synctime.h>
#include <utils/math/math.h>

using namespace nx;
using namespace nx::vms::client::desktop;

const QByteArray QnAuditLogModel::ChildCntParamName("childCnt");
const QByteArray QnAuditLogModel::CheckedParamName("checked");

namespace
{
    QString firstResourceName(const QnAuditRecord *d1)
    {
        auto resourcePool = qnClientCoreModule->resourcePool();

        if (d1->resources.empty())
            return QString();
        if (QnResourcePtr res = resourcePool->getResourceById(d1->resources[0]))
            return res->getName();
        else
            return QString();
    }

    QString firstResourceIp(const QnAuditRecord *d1)
    {
        auto resourcePool = qnClientCoreModule->resourcePool();

        if (d1->resources.empty())
            return QString();
        if (QnNetworkResourcePtr res = resourcePool->getResourceById<QnNetworkResource>(d1->resources[0]))
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
    DataIndex() : m_sortCol(TimestampColumn), m_sortOrder(Qt::DescendingOrder)
    {
    }

    void setSort(Column column, Qt::SortOrder order)
    {
        if ((Column)column == m_sortCol) {
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
        return m_sortOrder == Qt::AscendingOrder ? m_data[row] : m_data[m_data.size() - 1 - row];
    }

    // comparators

    typedef bool(*LessFunc)(const QnAuditRecord *d1, const QnAuditRecord *d2);

    static bool lessThanTimestamp(const QnAuditRecord *d1, const QnAuditRecord *d2)
    {
        return d1->createdTimeSec < d2->createdTimeSec;
    }

    static bool lessThanEndTimestamp(const QnAuditRecord *d1, const QnAuditRecord *d2)
    {
        const bool leftLoginSessionIsIncomplete =
            d1->rangeEndSec == 0 && d1->eventType == Qn::AR_Login;
        const bool rightLoginSessionIsIncomplete =
            d2->rangeEndSec == 0 && d2->eventType == Qn::AR_Login;

        if (leftLoginSessionIsIncomplete != rightLoginSessionIsIncomplete)
            return leftLoginSessionIsIncomplete;

        return leftLoginSessionIsIncomplete
            ? d1->createdTimeSec < d2->createdTimeSec
            : d1->rangeEndSec < d2->rangeEndSec;
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
        switch (m_sortCol)
        {
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
            default:
                break;
        }

        std::sort(m_data.begin(), m_data.end(), lessThan);
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

QnAuditLogModel::QnAuditLogModel(QObject *parent)
    : base_type(parent)
    , QnWorkbenchContextAware(parent)
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
    auto resourcePool = qnClientCoreModule->resourcePool();
    return QnResourceDisplayInfo(resourcePool->getResourceById(id)).toString(
        qnSettings->resourceInfoLevel());
}

QString QnAuditLogModel::formatDateTime(int timestampSecs, bool showDate, bool showTime) const
{
    if (timestampSecs == 0)
        return QString();

    // TODO: #sivanov Actualize used system context.
    const auto timeWatcher = appContext()->currentSystemContext()->serverTimeWatcher();
    QDateTime dateTime = timeWatcher->displayTime(timestampSecs * 1000ll);
    return formatDateTime(dateTime, showDate, showTime);
}

QString QnAuditLogModel::formatDateTime(const QDateTime& dateTime, bool showDate, bool showTime)
{
    if (showDate && showTime)
        return nx::vms::time::toString(dateTime);

    if (showDate)
        return nx::vms::time::toString(dateTime.date());

    if (showTime)
        return nx::vms::time::toString(dateTime.time());

    return QString();
}

QString QnAuditLogModel::formatDuration(int durationSecs)
{
    const auto duration = std::chrono::seconds(durationSecs);
    static const QString kSeparator(' ');

    using HumanReadable = nx::vms::text::HumanReadable;
    return HumanReadable::timeSpan(duration,
        HumanReadable::Days | HumanReadable::Hours | HumanReadable::Minutes,
        HumanReadable::SuffixFormat::Short,
        kSeparator,
        HumanReadable::kNoSuppressSecondUnit);
}

QString QnAuditLogModel::eventTypeToString(Qn::AuditRecordType eventType)
{
    auto resourcePool = qnClientCoreModule->resourcePool();
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
            return QnDeviceDependentStrings::getDefaultNameFromSet(
                resourcePool,
                tr("Device updated"),
                tr("Camera updated")
                );
        case Qn::AR_CameraInsert:
            return QnDeviceDependentStrings::getDefaultNameFromSet(
                resourcePool,
                tr("Device added"),
                tr("Camera added")
                );
        case Qn::AR_SystemNameChanged:
            return tr("System name changed");
        case Qn::AR_SystemmMerge:
            return tr("System merge");
        case Qn::AR_SettingsChange:
            return tr("General settings updated");
        case Qn::AR_ServerUpdate:
            return tr("Server updated");
        case Qn::AR_StorageInsert:
            return tr("Storage added");
        case Qn::AR_StorageUpdate:
            return tr("Storage updated");
        case Qn::AR_BEventUpdate:
            return tr("Event rule changed");
        case Qn::AR_EmailSettings:
            return tr("Email settings changed");
        case Qn::AR_CameraRemove:
            return QnDeviceDependentStrings::getDefaultNameFromSet(
                resourcePool,
                tr("Device removed"),
                tr("Camera removed")
                );
        case Qn::AR_StorageRemove:
            return tr("Storage removed");
        case Qn::AR_ServerRemove:
            return tr("Server removed");
        case Qn::AR_BEventRemove:
            return tr("Event rule removed");
        case Qn::AR_UserRemove:
            return tr("User removed");
        case Qn::AR_BEventReset:
            return tr("Event rules reset to default");
        case Qn::AR_DatabaseRestore:
            return tr("Database restored");
        case Qn::AR_UpdateInstall:
            return tr("Update installed");
        case Qn::AR_MitmAttack:
            return tr("MitM Attack");
        case Qn::AR_CloudBind:
            return nx::format(tr("Connected to %1", "%1 is the Cloud name (like Nx Cloud)"),
                nx::branding::cloudName());
        case Qn::AR_CloudUnbind:
            return nx::format(tr("Disconnected from %1", "%1 is the Cloud name (like Nx Cloud)"),
                nx::branding::cloudName());
    }
    return QString();
}

QnVirtualCameraResourceList QnAuditLogModel::getCameras(const QnAuditRecord* record)
{
    return record ? getCameras(record->resources) : QnVirtualCameraResourceList();
}

QnVirtualCameraResourceList QnAuditLogModel::getCameras(const std::vector<QnUuid>& resources)
{
    auto resourcePool = qnClientCoreModule->resourcePool();
    QnVirtualCameraResourceList result;
    for (const auto& id : resources)
        if (QnVirtualCameraResourcePtr camera = resourcePool->getResourceById<QnVirtualCameraResource>(id))
            result << camera;
    return result;
}

QString QnAuditLogModel::getResourcesString(const std::vector<QnUuid>& resources)
{
    QString result;
    for (const auto& res : resources)
    {
        if (!result.isEmpty())
            result += lit(",");
        result += getResourceNameById(res);
    }
    return result;
}

QString QnAuditLogModel::eventDescriptionText(const QnAuditRecord* data) const
{
    QString result;
    switch (data->eventType)
    {
        case Qn::AR_CameraRemove:
        case Qn::AR_StorageRemove:
            result = QString::fromUtf8(data->extractParam("description"));
            break;
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
            result = lit("%1 - %2, ").arg(formatDateTime(data->rangeStartSec)).arg(formatDateTime(data->rangeEndSec));
            /*fallthrough*/
        case Qn::AR_CameraUpdate:
        case Qn::AR_CameraInsert:
            result += QnDeviceDependentStrings::getNumericName(
                resourcePool(),
                getCameras(data->resources));
            break;
        case Qn::AR_UpdateInstall:
        {
            QString version = QString(data->extractParam("version"));
            result += QString("System has been updated to version %1").arg(version);
            break;
        }
        // TODO: add more info from certificates, like fingerprints.
        case Qn::AR_MitmAttack:
            result = nx::format(tr("MitM attack from server %1"), data->extractParam("serverId"));
            break;
        case Qn::AR_CloudBind:
            result = nx::format(tr("Connected to %1 via %2",
                "%1 is the Cloud name (like Nx Cloud), "
                    "%2 is a description of the agent used for establishing the connection "
                    "(like Nx Witness Desktop Client 5.2.0.0)"),
                nx::branding::cloudName(),
                data->extractParam("userAgent"));
            break;
        case Qn::AR_CloudUnbind:
            result = nx::format(
                tr("Disconnected from %1 via %2",
                    "%1 is the Cloud name (like Nx Cloud), "
                    "%2 is a description of the agent used for establishing the connection "
                    "(like Nx Witness Desktop Client 5.2.0.0)"),
                nx::branding::cloudName(),
                data->extractParam("userAgent"));
            break;
        default:
            result = getResourcesString(data->resources);
    }
    return result;
}

QString QnAuditLogModel::htmlData(const Column& column, const QnAuditRecord* data, int row, bool hovered) const
{
    using namespace nx::vms::common;

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
            result = lit("%1 - %2, ").arg(formatDateTime(data->rangeStartSec)).arg(formatDateTime(data->rangeEndSec));
            [[fallthrough]];
        case Qn::AR_CameraInsert:
        case Qn::AR_CameraUpdate:
        {
            const QColor linkColor = colorTheme()->color("auditLog.httpLink");

            QString txt = QnDeviceDependentStrings::getNumericName(
                resourcePool(),
                getCameras(data->resources));

            txt = html::bold(html::colored(txt, linkColor));
            if (hovered)
                txt = html::underlined(txt);
            result += txt;

            if (hasDetail(data))
            {
                auto archiveData = data->extractParam("archiveExist");
                size_t index = 0;
                for (const auto& camera : data->resources)
                {
                    result += html::kLineBreak;
                    if (data->isPlaybackType())
                    {
                        bool isRecordExist = archiveData.size() > index && archiveData[index] == '1';
                        index++;
                        QChar circleSymbol = isRecordExist ? QChar(0x25CF) : QChar(0x25CB);
                        if (isRecordExist)
                            result += QString("<font size=5 color=red>%1</font>").arg(circleSymbol);
                        else
                            result += QString("<font size=5>%1</font>").arg(circleSymbol);
                    }
                    result += getResourceNameById(camera);
                }
            }
            return result;
        }
        default:
            break;
        }
    }

    if (column == DateColumn && skipDate(data, row))
        return QString();
    else
        return textData(column, data);
}

bool QnAuditLogModel::matches(const QnAuditRecord* record, const QStringList& keywords) const
{
    static constexpr std::array<Column, 4> kColumnsToFilter =
        {
            UserNameColumn,
            UserHostColumn,
            EventTypeColumn,
            DescriptionColumn
        };

    // Every keyword must be present at least in one of the columns.
    for (const auto& keyword: keywords)
    {
        if (!std::any_of(kColumnsToFilter.cbegin(), kColumnsToFilter.cend(),
            [&] (auto column) { return matches(column, record, keyword); }))
        {
            return false;
        }
    }
    return true;
}

bool QnAuditLogModel::matches(
    const Column& column,
    const QnAuditRecord* data,
    const QString& keyword) const
{
    if (column == DescriptionColumn
        && (data->isPlaybackType()
            || data->eventType == Qn::AR_CameraUpdate || data->eventType == Qn::AR_CameraInsert))
    {
        for (const auto& resource: resourcePool()->getResourcesByIds(data->resources))
        {
            if (resources::search_helper::matches(keyword, resource))
                return true;
        }
    }

    return textData(column, data).contains(keyword, Qt::CaseInsensitive);
}

QString QnAuditLogModel::textData(const Column& column, const QnAuditRecord* data) const
{
    switch (column)
    {
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
            else if (data->eventType == Qn::AR_UnauthorizedLogin)
                return eventTypeToString(data->eventType);
            break;

        case DurationColumn:
            if (data->rangeEndSec)
                return formatDuration(data->rangeEndSec - data->rangeStartSec);
            else
                return QString();

        case UserNameColumn:
            return data->authSession.userName;

        case UserHostColumn:
            return data->authSession.userHost;

        case EventTypeColumn:
            return eventTypeToString(data->eventType);

        case CameraNameColumn:
            return firstResourceName(data);

        case CameraIpColumn:
            return firstResourceIp(data);

        case DescriptionColumn:
            return eventDescriptionText(data);

        case UserActivityColumn:
            return tr("%n actions", "", data->extractParam(ChildCntParamName).toUInt());

        default:
            break;
    }

    return QString();
}

void QnAuditLogModel::sort(int column, Qt::SortOrder order) {
    beginResetModel();
    m_index->setSort(m_columns[column], order);
    endResetModel();
}

int QnAuditLogModel::rowCount(const QModelIndex &parent) const {
    if (!parent.isValid())
        return m_index->size();
    return 0;
}

int QnAuditLogModel::columnCount(const QModelIndex &parent /* = QModelIndex()*/) const {
    if (!parent.isValid())
        return m_columns.size();
    return 0;
}

QVariant QnAuditLogModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole)
    {
        if (section >= m_columns.size())
            return QVariant();

        const Column &column = m_columns[section];
        switch (column)
        {
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
                return QnDeviceDependentStrings::getDefaultNameFromSet(
                    resourcePool(),
                    tr("Device name"),
                    tr("Camera name"));
            case CameraIpColumn:
                return tr("IP");
            case DateColumn:
                return tr("Date");
            case TimeColumn:
                return tr("Time");
            case DescriptionColumn:
                return tr("Description");
            default:
                break;
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
        if (m_index->at(i)->extractParam(CheckedParamName) == "1")
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
    emit dataChanged(index(0, 0), index(m_index->size(), m_columns.size()), QVector<int>() << Qt::CheckStateRole);
}

void QnAuditLogModel::setData(const QModelIndexList &indexList, const QVariant &value, int role)
{
    blockSignals(true);
    for (const auto& index : indexList)
        setData(index, value, role);
    blockSignals(false);
    emit dataChanged(index(0, 0), index(m_index->size(), m_columns.size()), QVector<int>() << role);
}

bool QnAuditLogModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || index.model() != this || !hasIndex(index.row(), index.column(), index.parent()))
        return false;

    if (role == Qt::CheckStateRole)
    {
        if (value.toInt() == Qt::Checked)
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
    for (const auto& record : m_index->data())
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
    case Qn::AR_MitmAttack: //< TODO: different color for MitM attack.
    case Qn::AR_UnauthorizedLogin:
        return colorTheme()->color("auditLog.actions.failedLogin");
    case Qn::AR_Login:
        return colorTheme()->color("auditLog.actions.login");
    case Qn::AR_UserUpdate:
    case Qn::AR_UserRemove:
        return colorTheme()->color("auditLog.actions.updateUser");
    case Qn::AR_ViewLive:
        return colorTheme()->color("auditLog.actions.watchingLive");
    case Qn::AR_ViewArchive:
        return colorTheme()->color("auditLog.actions.watchingArchive");
    case Qn::AR_ExportVideo:
        return colorTheme()->color("auditLog.actions.exportVideo");
    case Qn::AR_CameraUpdate:
    case Qn::AR_CameraRemove:
    case Qn::AR_CameraInsert:
        return colorTheme()->color("auditLog.actions.updateCamera");
    case Qn::AR_SystemNameChanged:
    case Qn::AR_SystemmMerge:
    case Qn::AR_SettingsChange:
    case Qn::AR_DatabaseRestore:
    case Qn::AR_UpdateInstall:
        return colorTheme()->color("auditLog.actions.system");
    case Qn::AR_StorageInsert:
    case Qn::AR_StorageUpdate:
    case Qn::AR_StorageRemove:
    case Qn::AR_ServerUpdate:
    case Qn::AR_ServerRemove:
        return colorTheme()->color("auditLog.actions.updateServer");
    case Qn::AR_BEventReset:
    case Qn::AR_BEventUpdate:
    case Qn::AR_BEventRemove:
        return colorTheme()->color("auditLog.actions.rules");
    case Qn::AR_EmailSettings:
        return colorTheme()->color("auditLog.actions.emailSettings");
    default:
        return QVariant();
    }
}

bool QnAuditLogModel::skipDate(const QnAuditRecord *record, int row) const
{
    if (row < 1)
        return false;

    // TODO: #sivanov Actualize used system context.
    const auto timeWatcher = appContext()->currentSystemContext()->serverTimeWatcher();
    QDate d1 = timeWatcher->displayTime(record->createdTimeSec * 1000).date();
    QDate d2 = timeWatcher->displayTime(m_index->at(row - 1)->createdTimeSec * 1000).date();
    return d1 == d2;
}

bool QnAuditLogModel::isDetailDataSupported(const QnAuditRecord *record) const
{
    switch (record->eventType)
    {
    case Qn::AR_ViewArchive:
    case Qn::AR_ViewLive:
    case Qn::AR_ExportVideo:
    case Qn::AR_CameraInsert:
    case Qn::AR_CameraUpdate:
        return true;
    default:
        return false;
    }
    return false;
}

QString QnAuditLogModel::descriptionTooltip(const QnAuditRecord *record) const
{
    if (record->resources.empty() || !isDetailDataSupported(record))
        return QString();

    QString result;
    if (!hasDetail(record))
        result = tr("Click to expand");
    if (!record->isPlaybackType())
        return result;
    if (!result.isEmpty())
        result += lit("\n");
    result += tr("Filled circle mark means the archive is still available");
    return result;
}

QVariant QnAuditLogModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.model() != this || !hasIndex(index.row(), index.column(), index.parent()))
        return QVariant();

    const Column &column = m_columns[index.column()];

    const QnAuditRecord *record = m_index->at(index.row());

    switch (role)
    {
    case Qt::CheckStateRole:
        if (column == SelectRowColumn)
            return record->extractParam(CheckedParamName) == "1" ? Qt::Checked : Qt::Unchecked;
        else
            return QVariant();

    case Qt::DisplayRole:
        if (column == DateColumn && skipDate(record, index.row()))
            return QString();
        else
            return QVariant(textData(column, record));

    case Qn::DisplayHtmlRole:
        return htmlData(column, record, index.row(), false);

    case Qn::AuditRecordDataRole:
        return QVariant::fromValue<QnAuditRecord*>(m_index->at(index.row()));

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
        if (column == DateColumn)
        {
            QFont font;
            font.setBold(true);
            return font;
        }
        return QVariant();

    case Qt::DecorationRole:
    case Qn::DecorationHoveredRole:
        {
            if (column != DescriptionColumn)
                return QVariant();

            if (record->isPlaybackType())
                    return qnSkin->icon("audit/play.png");

            switch (record->eventType)
            {
            case Qn::AR_UserUpdate:
                return qnSkin->icon("tree/user.svg");

            case Qn::AR_StorageInsert:
            case Qn::AR_StorageUpdate:
            case Qn::AR_ServerUpdate:
                return qnSkin->icon("tree/server.png");

            case Qn::AR_CameraInsert:
            case Qn::AR_CameraUpdate:
                return qnSkin->icon("tree/camera.svg");

            default:
                return QVariant();
            }
        }

    case Qt::ToolTipRole:
        if (column == DescriptionColumn)
            return descriptionTooltip(record);
        break;
    }

    return QVariant();
}

void QnAuditLogModel::setColumns(const QList<Column> &columns)
{
    m_columns = columns;
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
