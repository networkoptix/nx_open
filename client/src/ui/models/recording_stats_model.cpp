#include "recording_stats_model.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/resource_name.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource/camera_resource.h>

#include <ui/common/ui_resource_name.h>
#include <ui/style/resource_icon_cache.h>

#include <utils/common/string.h>

namespace {
    const qreal BYTES_IN_GB = 1000000000.0;
    const qreal BYTES_IN_TB = 1000.0 * BYTES_IN_GB;
    const qreal SECS_PER_HOUR = 3600.0;
    const qreal SECS_PER_DAY = SECS_PER_HOUR * 24;
    const qreal SECS_PER_YEAR = SECS_PER_DAY * 365.0;
    const qreal SECS_PER_MONTH = SECS_PER_YEAR / 12.0;
    const qreal BYTES_IN_MB = 1000000.0;
    const int PREC = 2;

    /* One additional row for footer. */
    const int footerRowsCount = 1;

    const int kMaxNameLength = 30;
}

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
        return naturalStringLess(left.data(Qt::DisplayRole).toString(), right.data(Qt::DisplayRole).toString());
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
    m_isForecastRole(isForecastRole)
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
            QString foreignText = tr("<Cameras from other servers and removed cameras>");
            int maxLength = qMax(foreignText.length(), kMaxNameLength); /* There is no need to limit name to be shorter than predefined string. */
            return isForeign
                ? foreignText
                : elideString(getResourceName(qnResPool->getResourceByUniqueId(value.uniqueId)), maxLength);
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
                if (QnVirtualCameraResourcePtr camera = qnResPool->getResourceByUniqueId<QnVirtualCameraResource>(data.uniqueId))
                    cameras << camera;
            //NX_ASSERT(cameras.size() == m_data.size(), Q_FUNC_INFO, "Make sure all cameras exist");
            return QnDeviceDependentStrings::getNameFromSet(
                QnCameraDeviceStringSet(
                    tr("Total %n devices",      "", cameras.size()),
                    tr("Total %n cameras",      "", cameras.size()),
                    tr("Total %n I/O modules",  "", cameras.size())
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
        return qnResPool->getResourceByUniqueId(value.uniqueId);
    default:
        return QnResourcePtr();
    }
}

qreal QnRecordingStatsModel::chartData(const QModelIndex &index) const
{
    const QnCamRecordingStatsData& value = m_data.at(index.row());
    qreal result = 0.0;
    const auto& footer = m_footer;
    switch(index.column())
    {
    case BytesColumn:
        if (footer.recordedBytes > 0)
            result = value.recordedBytes / (qreal) footer.recordedBytes;
        else
            return 0.0;
        break;
    case DurationColumn:
        if (footer.archiveDurationSecs > 0)
            result = value.archiveDurationSecs / (qreal) footer.archiveDurationSecs;
        else
            result = 0.0;
        break;
    case BitrateColumn:
        if (footer.averageBitrate > 0)
            result =  value.averageBitrate / (qreal) footer.averageBitrate;
        else
            result =  0.0;
        break;
    }
    NX_ASSERT(qBetween(0.0, result, 1.00001));
    return qBound(0.0, result, 1.0);
}

QVariant QnRecordingStatsModel::footerData(const QModelIndex &index, int role) const
{
    switch(role) {
    case Qt::DisplayRole:
    case Qn::DisplayHtmlRole:
        return footerDisplayData(index);
    case Qt::ToolTipRole:
        return tooltipText(static_cast<Columns>(index.column()));
    case Qt::FontRole: {
        QFont font;
        font.setBold(true);
        return font;
    }
    case Qt::BackgroundRole:
        return qApp->palette().color(QPalette::Normal, QPalette::Background);

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
                tr("Devices with non-empty archive"),
                tr("Cameras with non-empty archive")
                );
        case BytesColumn:
            return QnDeviceDependentStrings::getDefaultNameFromSet(
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
    if (!index.isValid() || index.model() != this || !hasIndex(index.row(), index.column(), index.parent()))
        return QVariant();

    if (index.row() >= m_data.size())
        return footerData(index, role);

    switch(role) {
    case Qt::DisplayRole:
    case Qn::DisplayHtmlRole:
        return displayData(index);
    case Qt::DecorationRole:
        return qnResIconCache->icon(getResource(index));
    case Qn::ResourceRole:
        return QVariant::fromValue<QnResourcePtr>(getResource(index));
    case Qt::BackgroundRole:
        return qApp->palette().color(QPalette::Normal, QPalette::Base);
    case Qn::RecordingStatsDataRole:
        return QVariant::fromValue<QnCamRecordingStatsData>(m_data.at(index.row()));
    case Qn::RecordingStatChartDataRole:
        return chartData(index);
    //case Qn::RecordingStatForecastDataRole:
    //    return chartData(index, true);
    //case Qn::RecordingStatColorsDataRole:
    //    return QVariant::fromValue<QnRecordingStatsColors>(m_colors);
    case Qn::RecordingStatChartColorDataRole:
        return m_isForecastRole ? m_colors.chartForecastColor : m_colors.chartMainColor;
    case Qt::ToolTipRole:
        return tooltipText(static_cast<Columns>(index.column()));
    default:
        break;
    }
    return QVariant();
}

QVariant QnRecordingStatsModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || section >= ColumnCount)
        return base_type::headerData(section, orientation, role);

    if (role == Qt::DisplayRole) {
        switch(section) {
        case CameraNameColumn: return QnDeviceDependentStrings::getDefaultNameFromSet(tr("Device"), tr("Camera"));
        case BytesColumn:      return tr("Space");
        case DurationColumn:   return tr("Calendar Days");
        case BitrateColumn:    return QVariant(); //return tr("Bitrate");
        default:
            break;
        }
    }
    //else if (role == Qt::TextAlignmentRole)
    //    return Qt::AlignLeft; // it's broken at our style and value is ignored

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

QnRecordingStatsColors QnRecordingStatsModel::colors() const {
    return m_colors;
}

void QnRecordingStatsModel::setColors(const QnRecordingStatsColors &colors) {
    m_colors = colors;
    emit colorsChanged();
}

QnFooterData QnRecordingStatsModel::calculateFooter(const QnRecordingStatsReply& data) const {
    QnRecordingStatsData summ;
    QnRecordingStatsData maxValue;
    QnFooterData footer;

    for(const QnCamRecordingStatsData& value: data) {
        summ += value;
        maxValue.recordedBytes = qMax(maxValue.recordedBytes, value.recordedBytes);
        maxValue.recordedSecs = qMax(maxValue.recordedSecs, value.recordedSecs);
        maxValue.archiveDurationSecs = qMax(maxValue.archiveDurationSecs, value.archiveDurationSecs);
        maxValue.averageBitrate = qMax(maxValue.averageBitrate, value.averageBitrate);
        footer.bitrateSum += value.averageBitrate;
    }

    footer.recordedBytes = summ.recordedBytes;
    footer.recordedSecs = summ.recordedSecs;
    footer.archiveDurationSecs = summ.archiveDurationSecs;
    footer.averageBitrate = maxValue.averageBitrate;

    return footer;
}

QString QnRecordingStatsModel::formatBitrateString(qint64 bitrate) const {
    if (bitrate > 0)
        return tr("%1 Mbps").arg(QString::number(bitrate / BYTES_IN_MB * 8, 'f', PREC));

    return lit("-");
}

QString QnRecordingStatsModel::formatBytesString(qint64 bytes) const {
    if (bytes > BYTES_IN_TB)
        return tr("%1 Tb").arg(QString::number(bytes / BYTES_IN_TB, 'f', PREC));
    else
        return tr("%1 Gb").arg(QString::number(bytes / BYTES_IN_GB, 'f', PREC));
}


QString QnRecordingStatsModel::formatDurationString(const QnCamRecordingStatsData &data) const
{
    if (data.archiveDurationSecs == 0)
        return tr("empty");

    qint64 tmpVal = data.archiveDurationSecs;
    int years = tmpVal / SECS_PER_YEAR;
    tmpVal -= years * SECS_PER_YEAR;
    int months = tmpVal / SECS_PER_MONTH;
    tmpVal -= months * SECS_PER_MONTH;
    int days = tmpVal / SECS_PER_DAY;
    tmpVal -= days * SECS_PER_DAY;
    int hours = tmpVal / SECS_PER_HOUR;
    QString result;
    static const QString DELIM(lit(" "));
    if (years > 0)
        result += tr("%n years", "", years);
    if (months > 0) {
        if (!result.isEmpty())
            result += DELIM;
        result += tr("%n months", "", months);
    }
    if (days > 0) {
        if (!result.isEmpty())
            result += DELIM;
        result += tr("%n days", "", days);
    }
    if (hours > 0 && years == 0) {
        if (!result.isEmpty())
            result += DELIM;
        result += tr("%n hours", "", hours);
    }
    if (result.isEmpty())
        result = tr("less than an hour");
    return result;
}
