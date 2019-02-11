#include "recording_stats_model.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource/camera_resource.h>
#include <core/resource/resource_display_info.h>
#include <core/resource/media_server_resource.h>

#include <client/client_settings.h>

#include <ui/style/resource_icon_cache.h>

#include <nx/utils/string.h>

#include <nx/client/core/utils/human_readable.h>

namespace {

// TODO: #GDM #vkutin #common Refactor all this to use HumanReadable helper class
const qreal kBytesInKB = 1024.0;

const qreal kBytesInMB = kBytesInKB * kBytesInKB;
const qreal kBytesInGB = kBytesInMB * kBytesInKB;
const qreal kBytesInTB = kBytesInGB * kBytesInKB;

const qreal kSecondsPerHour = 3600.0;
const qreal kSecondsPerDay = kSecondsPerHour * 24;
const qreal kSecondsPerYear = kSecondsPerDay * 365.25;
const qreal kSecondsPerMonth = kSecondsPerYear / 12.0;

const int kPrecision = 2;

} // namespace

bool QnSortedRecordingStatsModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    const auto leftRow = left.data(QnRecordingStatsModel::RowKind).value<QnRecordingStatsModel::RowType>();
    const auto rightRow = right.data(QnRecordingStatsModel::RowKind).value<QnRecordingStatsModel::RowType>();
    if (leftRow == QnRecordingStatsModel::Totals || rightRow == QnRecordingStatsModel::Totals) //< Totals last.
        return (leftRow == QnRecordingStatsModel::Totals) != (sortOrder() == Qt::AscendingOrder);

    if (leftRow == QnRecordingStatsModel::Foreign || rightRow == QnRecordingStatsModel::Foreign) //< Foreign last.
        return (leftRow == QnRecordingStatsModel::Foreign) != (sortOrder() == Qt::AscendingOrder);

    const auto leftData = left.data(QnRecordingStatsModel::StatsData).value<QnCamRecordingStatsData>();
    const auto rightData = right.data(QnRecordingStatsModel::StatsData).value<QnCamRecordingStatsData>();

    switch(left.column())
    {
        case QnRecordingStatsModel::CameraNameColumn:
            return nx::utils::naturalStringLess(left.data(Qt::DisplayRole).toString(), right.data(Qt::DisplayRole).toString());
        case QnRecordingStatsModel::BytesColumn:
            return leftData.recordedBytes < rightData.recordedBytes;
        case QnRecordingStatsModel::DurationColumn:
            return leftData.archiveDurationSecs < rightData.archiveDurationSecs;
        case QnRecordingStatsModel::BitrateColumn:
            return leftData.averageBitrate < rightData.averageBitrate;
        default:
            return false;
    }
}

bool QnSortedRecordingStatsModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    return  sourceModel()->index(sourceRow, 0, sourceParent).
        data(QnRecordingStatsModel::RowKind) != QnRecordingStatsModel::Totals;
}

bool QnTotalRecordingStatsModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    return  sourceModel()->index(sourceRow, 0, sourceParent).
        data(QnRecordingStatsModel::RowKind) == QnRecordingStatsModel::Totals;
}

const QnCamRecordingStatsData& QnCameraStatsData::getStatsForRow(int row) const
{
    if (row < cameras.size())
        return cameras[row];

    if (row == cameras.size())
        return showForeignCameras ? foreignCameras : totals;

    NX_ASSERT(showForeignCameras && row == cameras.size() + 1, "Row is out of range.");
    return totals;
}

QnRecordingStatsModel::QnRecordingStatsModel(bool isForecastRole, QObject *parent):
    base_type(parent),
    m_isForecastRole(isForecastRole)
{
}

QnRecordingStatsModel::~QnRecordingStatsModel()
{ }

int QnRecordingStatsModel::rowCount(const QModelIndex &parent) const
{
    int dataSize = 0;
    if (!parent.isValid())
    {
        dataSize = m_data.cameras.size();
        if (dataSize > 0)
            dataSize++;  // Add footer row.
        if (m_data.showForeignCameras)
            dataSize++;  // Add row for foreign cameras.
    }
    return dataSize;
}

int QnRecordingStatsModel::columnCount(const QModelIndex &parent) const
{
    if (!parent.isValid())
    {
        if (m_isForecastRole)
            return ColumnCount - 1; // Do not show "(Current) Bitrate" column.

        return ColumnCount;
    }

    return 0;
}

QString QnRecordingStatsModel::displayData(const QModelIndex &index) const
{
    const auto rowType = data(index, RowKind).value<RowType>();
    const auto& value = m_data.getStatsForRow(index.row());

    switch(index.column())
    {
        case CameraNameColumn:
        {
            if (rowType == Foreign || rowType == Totals)
                return value.uniqueId; //< We use value.uniqueId as name for pseudo-cameras.

            return nx::utils::elideString(
                QnResourceDisplayInfo(resourcePool()->getResourceByUniqueId(value.uniqueId)).toString(qnSettings->extraInfoInTree()),
                m_data.maxNameLength);
        }
        case BytesColumn: return formatBytesString(value.recordedBytes);
        case DurationColumn: return rowType != Normal ? QString() : formatDurationString(value);
        case BitrateColumn: return formatBitrateString(value.averageBitrate);
        default: return QString();
    }
}

qreal QnRecordingStatsModel::chartData(const QModelIndex& index) const
{
    if (data(index, RowKind).value<RowType>() == Totals)
        return 0.0; //< No chart for "totals" row.

    const auto& value = m_data.getStatsForRow(index.row());;
    qreal result = 0.0;

    const auto& scale = m_data.chartScale;

    switch (index.column())
    {
        case BytesColumn:
            if (scale.recordedBytes <= 0)
                return 0.0;
            result = value.recordedBytes / (qreal) scale.recordedBytes;
            break;

        case DurationColumn:
            if (scale.archiveDurationSecs <= 0)
                 return 0.0;
            result = value.archiveDurationSecs / (qreal) scale.archiveDurationSecs;
            break;

        case BitrateColumn:
            if (scale.averageBitrate <= 0)
                return 0.0;
            result = value.averageBitrate / (qreal) scale.averageBitrate;
            break;
        default: result = 0.0;
    }

    result = qBound(0.0, result, 1.0);
    NX_ASSERT(qBetween(0.0, result, 1.00001));
    return result;
}

QString QnRecordingStatsModel::tooltipText(Columns column) const
{
    return QString(); //< We currently decided to skip tooltips.
}

QVariant QnRecordingStatsModel::data(const QModelIndex &index, int role) const
{
    /* Check invalid indices. */
    if (!index.isValid() || index.model() != this || !hasIndex(index.row(), index.column(), index.parent()))
        return QVariant();

    RowType rowType = Normal;
    if (index.row() == m_data.cameras.size())
        rowType = m_data.showForeignCameras ? Foreign : Totals;
    if (index.row() >= m_data.cameras.size() + 1)
        rowType = Totals;

    switch(role)
    {
        case Qt::DisplayRole:
            return displayData(index);
        case Qt::DecorationRole:
        {
            if (index.column() != CameraNameColumn)
                return QVariant();

            if (rowType == Totals)
                return qnResIconCache->icon(QnResourceIconCache::Cameras | QnResourceIconCache::AlwaysSelected);

            if (rowType == Normal)
                return qnResIconCache->icon(resourcePool()->getResourceByUniqueId(m_data.cameras[index.row()].uniqueId));

            break;
        }
        case Qn::ResourceRole: //< This is used by QnResourceItemDelegate to draw contents.
        {
            if (rowType == Normal)
                return QVariant::fromValue<QnResourcePtr>(
                    resourcePool()->getResourceByUniqueId(m_data.cameras[index.row()].uniqueId));

            break;
        }
        case RowKind:
            return rowType;

        case StatsData:
            return QVariant::fromValue<QnCamRecordingStatsData>(m_data.getStatsForRow(index.row()));
        case ChartData:
            return chartData(index); //< Returns qreal.
        case Qt::ToolTipRole:
            return tooltipText(static_cast<Columns>(index.column()));
        default:
            break;
    }
    return QVariant();
}

QVariant QnRecordingStatsModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (m_isHeaderTextBlocked)
        return QVariant();

    if (orientation != Qt::Horizontal || section >= ColumnCount)
        return base_type::headerData(section, orientation, role);

    if (role == Qt::DisplayRole)
    {
        switch (section)
        {
            case CameraNameColumn:
                return QnDeviceDependentStrings::getDefaultNameFromSet(
                    resourcePool(),
                    tr("Device"),
                    tr("Camera"));
            case BytesColumn: return tr("Space");
            case DurationColumn: return tr("Calendar Days");
            case BitrateColumn: return tr("Current Bitrate");
            default: break;
        }
    }

    return base_type::headerData(section, orientation, role);
}

void QnRecordingStatsModel::clear()
{
    beginResetModel();
    m_data = QnCameraStatsData();
    endResetModel();
}

void QnRecordingStatsModel::setModelData(const QnCameraStatsData& data)
{
    beginResetModel();
    m_data = data;
    endResetModel();
}

QnCameraStatsData QnRecordingStatsModel::modelData() const
{
    return m_data;
}

void QnRecordingStatsModel::setHeaderTextBlocked(bool value)
{
    m_isHeaderTextBlocked = value;
}

QString QnRecordingStatsModel::formatBitrateString(qint64 bitrate) const
{
    if (bitrate > 0)
        return tr("%1 Mbps").arg(QString::number(bitrate / kBytesInMB * 8, 'f', kPrecision));

    return lit("-");
}

QString QnRecordingStatsModel::formatBytesString(qint64 bytes) const
{
    if (bytes > kBytesInTB)
        return tr("%1 TB").arg(QString::number(bytes / kBytesInTB, 'f', kPrecision));
    else
        return tr("%1 GB").arg(QString::number(bytes / kBytesInGB, 'f', kPrecision));
}

QString QnRecordingStatsModel::formatDurationString(const QnCamRecordingStatsData& data) const
{
    if (data.archiveDurationSecs == 0)
        return m_isForecastRole ? tr("no data for forecast") : tr("empty");

    if (data.archiveDurationSecs < kSecondsPerHour)
        return tr("less than an hour");

    const auto duration = std::chrono::seconds(data.archiveDurationSecs);
    static const QString kSeparator(L' ');

    using HumanReadable = nx::vms::client::core::HumanReadable;
    return HumanReadable::timeSpan(duration,
        HumanReadable::Years | HumanReadable::Months | HumanReadable::Days | HumanReadable::Hours,
        HumanReadable::SuffixFormat::Full,
        kSeparator,
        HumanReadable::kNoSuppressSecondUnit);
}
