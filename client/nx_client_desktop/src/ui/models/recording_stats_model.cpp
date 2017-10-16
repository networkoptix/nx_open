#include "recording_stats_model.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource/camera_resource.h>
#include <core/resource/resource_display_info.h>

#include <client/client_settings.h>

#include <ui/style/resource_icon_cache.h>

#include <nx/utils/string.h>

#include <utils/common/synctime.h>
#include <nx/client/core/utils/human_readable.h>

namespace {

// TODO: #vkutin #common Refactor all this to use some common formatting
// TODO: #GDM #3.1 move out strings and logic to separate class (string.h:bytesToString)
const qreal kBytesInKB = 1024.0;

const qreal kBytesInMB = kBytesInKB * kBytesInKB;
const qreal kBytesInGB = kBytesInMB * kBytesInKB;
const qreal kBytesInTB = kBytesInGB * kBytesInKB;

const qreal kSecondsPerHour = 3600.0;
const qreal kSecondsPerDay = kSecondsPerHour * 24;
const qreal kSecondsPerYear = kSecondsPerDay * 365.25;
const qreal kSecondsPerMonth = kSecondsPerYear / 12.0;

const int kPrecision = 2;

/* One additional row for footer. */
const int footerRowsCount = 1;

const int kMaxNameLength = 30;

} // namespace

const QString QnSortedRecordingStatsModel::kForeignCameras(lit("C7139D2D-0CB2-424D-9C73-704C417B32F2"));

bool QnSortedRecordingStatsModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    QnCamRecordingStatsData leftData = left.data(Qn::RecordingStatsDataRole).value<QnCamRecordingStatsData>();
    QnCamRecordingStatsData rightData = right.data(Qn::RecordingStatsDataRole).value<QnCamRecordingStatsData>();

    bool isNulIDLeft = leftData.uniqueId.isNull();
    bool isNulIDRight = rightData.uniqueId.isNull();
    if (isNulIDLeft != isNulIDRight) {
        if (sortOrder() == Qt::AscendingOrder)
            return isNulIDLeft < isNulIDRight; // keep footer without ID at the last place
        else
            return isNulIDLeft > isNulIDRight; // keep footer without ID at the last place
    }

    bool isForeignLeft = leftData.uniqueId == kForeignCameras;
    bool isForeignRight = rightData.uniqueId == kForeignCameras;
    if (isForeignLeft != isForeignRight) {
        if (sortOrder() == Qt::AscendingOrder)
            return isForeignLeft < isForeignRight;
        else
            return isForeignLeft > isForeignRight;
    }

    switch(left.column()) {
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

QnRecordingStatsModel::QnRecordingStatsModel(bool isForecastRole, QObject *parent):
    base_type(parent),
    m_isForecastRole(isForecastRole),
    m_isHeaderTextBlocked(false)
{ }

QnRecordingStatsModel::~QnRecordingStatsModel()
{ }

int QnRecordingStatsModel::rowCount(const QModelIndex &parent) const {
    if (!parent.isValid()) {
        int dataSize = m_data.size();
        if (dataSize > 0)
            return dataSize + footerRowsCount;
        return 0;
    }
    return 0;
}

int QnRecordingStatsModel::columnCount(const QModelIndex &parent) const {
    if (!parent.isValid())
        return ColumnCount;
    return 0;
}

QString QnRecordingStatsModel::displayData(const QModelIndex &index) const {
    const QnCamRecordingStatsData& value = m_data.at(index.row());
    bool isForeign = (value.uniqueId == QnSortedRecordingStatsModel::kForeignCameras);
    switch(index.column())
    {
    case CameraNameColumn:
        {
            QString foreignText = tr("Cameras from other servers and removed cameras");
            int maxLength = qMax(foreignText.length(), kMaxNameLength); /* There is no need to limit name to be shorter than predefined string. */
            return isForeign
                ? foreignText
                : nx::utils::elideString(
                    QnResourceDisplayInfo(resourcePool()->getResourceByUniqueId(value.uniqueId)).toString(qnSettings->extraInfoInTree()),
                    maxLength);
        }
    case BytesColumn:
        return formatBytesString(value.recordedBytes);
    case DurationColumn:
        return isForeign ? QString() : formatDurationString(value);
    case BitrateColumn:
        return isForeign ? QString() : formatBitrateString(value.averageBitrate);
    default:
        return QString();
    }
}

QString QnRecordingStatsModel::footerDisplayData(const QModelIndex &index) const {
    switch(index.column())
    {
    case CameraNameColumn:
        {
            QnVirtualCameraResourceList cameras;
            for (const QnCamRecordingStatsData &data: m_data)
                if (QnVirtualCameraResourcePtr camera = resourcePool()->getResourceByUniqueId<QnVirtualCameraResource>(data.uniqueId))
                    cameras << camera;
            //NX_ASSERT(cameras.size() == m_data.size(), Q_FUNC_INFO, "Make sure all cameras exist");
            static const auto kNDash = QString::fromWCharArray(L"\x2013");
            return QnDeviceDependentStrings::getNameFromSet(
                resourcePool(),
                QnCameraDeviceStringSet(
                    tr("Total %1 %n devices", "%1 is long dash, do not replace",
                        cameras.size()).arg(kNDash),
                    tr("Total %1 %n cameras", "%1 is long dash, do not replace",
                        cameras.size()).arg(kNDash),
                    tr("Total %1 %n I/O modules", "%1 is long dash, do not replace",
                        cameras.size()).arg(kNDash)
                ), cameras
            );
        }
    case BytesColumn:
        return formatBytesString(m_footer.recordedBytes);
    case DurationColumn:
        return formatDurationString(m_footer);
    case BitrateColumn:
        return formatBitrateString(m_footer.bitrateSum);
    default:
        return QString();
    }
    return QString();
}

QnResourcePtr QnRecordingStatsModel::getResource(const QModelIndex &index) const
{
    const QnCamRecordingStatsData& value = m_data.at(index.row());
    switch(index.column())
    {
    case CameraNameColumn:
        return resourcePool()->getResourceByUniqueId(value.uniqueId);
    default:
        return QnResourcePtr();
    }
}

qreal QnRecordingStatsModel::chartData(const QModelIndex& index) const
{
    const QnCamRecordingStatsData& value = m_data.at(index.row());
    qreal result = 0.0;

    const auto& footer = m_footer;
    switch (index.column())
    {
        case BytesColumn:
            if (footer.maxValues.recordedBytes <= 0)
                return 0.0;
            result = value.recordedBytes / (qreal) footer.maxValues.recordedBytes;
            break;

        case DurationColumn:
            if (footer.maxValues.archiveDurationSecs <= 0)
                 return 0.0;
            result = value.archiveDurationSecs / (qreal) footer.maxValues.archiveDurationSecs;
            break;

        case BitrateColumn:
            if (footer.maxValues.averageBitrate <= 0)
                return 0.0;
            result = value.averageBitrate / (qreal) footer.maxValues.averageBitrate;
            break;
    }

    result = qBound(0.0, result, 1.0);
    NX_ASSERT(qBetween(0.0, result, 1.00001));
    return result;
}

QVariant QnRecordingStatsModel::footerData(const QModelIndex &index, int role) const
{
    switch(role)
    {
        case Qt::DisplayRole:
            return footerDisplayData(index);
        case Qt::ToolTipRole:
            return tooltipText(static_cast<Columns>(index.column()));
        case Qt::DecorationRole:
        {
            if (index.column() != CameraNameColumn)
                break;

            // Makes always-selected cameras icon
            const auto source = qnResIconCache->icon(QnResourceIconCache::Cameras);
            QIcon result;
            for (const auto& size: source.availableSizes(QIcon::Selected))
            {
                const auto selectedPixmap = source.pixmap(size, QIcon::Selected);
                result.addPixmap(selectedPixmap, QIcon::Normal);
                result.addPixmap(selectedPixmap, QIcon::Selected);
            }
            return result;
        }
        case Qn::RecordingStatsDataRole:
            return QVariant::fromValue<QnCamRecordingStatsData>(m_footer);
        default:
            break;
    }

    return QVariant();
}

QString QnRecordingStatsModel::tooltipText(Columns column) const
{
    switch (column)
    {
        case CameraNameColumn:
            return QnDeviceDependentStrings::getDefaultNameFromSet(
                resourcePool(),
                tr("Devices with non-empty archive"),
                tr("Cameras with non-empty archive")
                );
        case BytesColumn:
            return QnDeviceDependentStrings::getDefaultNameFromSet(
                resourcePool(),
                tr("Storage space occupied by devices"),
                tr("Storage space occupied by cameras")
                );
        case DurationColumn:
            return tr("Archived duration in calendar days since the first recording");
        case BitrateColumn:
            return tr("Average bitrate for the recorded period");
        default:
            break;
    }
    return QString();
}

QVariant QnRecordingStatsModel::data(const QModelIndex &index, int role) const
{
    /* Check invalid indices. */
    if (!index.isValid() || index.model() != this
        || !hasIndex(index.row(), index.column(), index.parent()))
    {
        return QVariant();
    }

    if (index.row() >= m_data.size())
        return footerData(index, role);

    switch(role)
    {
        case Qt::DisplayRole:
            return displayData(index);
        case Qt::DecorationRole:
            return qnResIconCache->icon(getResource(index));
        case Qn::ResourceRole:
            return QVariant::fromValue<QnResourcePtr>(getResource(index));
        case Qn::RecordingStatsDataRole:
            return QVariant::fromValue<QnCamRecordingStatsData>(m_data.at(index.row()));
        case Qn::RecordingStatChartDataRole:
            return chartData(index);
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
            case BytesColumn:      return tr("Space");
            case DurationColumn:   return tr("Calendar Days");
            case BitrateColumn:    return tr("Bitrate");
            default:               break;
        }
    }

    return base_type::headerData(section, orientation, role);
}

void QnRecordingStatsModel::clear() {
    beginResetModel();
    m_data.clear();
    endResetModel();
}

void QnRecordingStatsModel::setModelData(const QnRecordingStatsReply& data) {
    beginResetModel();
    m_data = data;
    m_footer = calculateFooter(data);
    endResetModel();
}

QnRecordingStatsReply QnRecordingStatsModel::modelData() const {
    return m_data;
}

void QnRecordingStatsModel::setHeaderTextBlocked(bool value)
{
    m_isHeaderTextBlocked = value;
}

QnFooterData QnRecordingStatsModel::calculateFooter(const QnRecordingStatsReply& data) const {
    QnRecordingStatsData summ;
    QnFooterData footer;

    for(const QnCamRecordingStatsData& value: data) {
        summ += value;
        footer.maxValues.recordedBytes = qMax(footer.maxValues.recordedBytes, value.recordedBytes);
        footer.maxValues.recordedSecs = qMax(footer.maxValues.recordedSecs, value.recordedSecs);
        footer.maxValues.archiveDurationSecs = qMax(footer.maxValues.archiveDurationSecs, value.archiveDurationSecs);
        footer.maxValues.averageBitrate = qMax(footer.maxValues.averageBitrate, value.averageBitrate);
        footer.bitrateSum += value.averageBitrate;
    }

    footer.recordedBytes = summ.recordedBytes;
    footer.recordedSecs = summ.recordedSecs;
    footer.archiveDurationSecs = summ.archiveDurationSecs;
    footer.averageBitrate = footer.maxValues.averageBitrate;

    return footer;
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
        return tr("empty");

    if (data.archiveDurationSecs < kSecondsPerHour)
        return tr("less than an hour");

    const auto duration = std::chrono::seconds(data.archiveDurationSecs);
    static const QString kSeparator(L' ');

    using HumanReadable = nx::client::core::HumanReadable;
    return HumanReadable::timeSpan(duration,
        HumanReadable::Years | HumanReadable::Months | HumanReadable::Days | HumanReadable::Hours,
        HumanReadable::SuffixFormat::Full,
        kSeparator,
        HumanReadable::kNoSuppressSecondUnit);
}
