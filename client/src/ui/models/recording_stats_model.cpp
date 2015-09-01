#include "recording_stats_model.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/resource_name.h>

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
}

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

QnRecordingStatsModel::QnRecordingStatsModel(QObject *parent) :
    base_type(parent)
{ }

QnRecordingStatsModel::~QnRecordingStatsModel()
{ }

int QnRecordingStatsModel::rowCount(const QModelIndex &parent) const {
    if (!parent.isValid()) {
        int dataSize = internalModelData().size();
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
    const QnRecordingStatsReply& data = internalModelData();
    const QnCamRecordingStatsData& value = data.at(index.row());
    switch(index.column()) {
    case CameraNameColumn:
        return getResourceName(qnResPool->getResourceByUniqueId(value.uniqueId));
    case BytesColumn:
        return formatBytesString(value.recordedBytes);
    case DurationColumn:
        return formatDurationString(value);           
    case BitrateColumn:
        return formatBitrateString(value.averageBitrate);
    default:
        return QString();
    }
}

QString QnRecordingStatsModel::footerDisplayData(const QModelIndex &index) const {
    auto footer = internalFooterData();

    switch(index.column())
    {
    case CameraNameColumn:
        return tr("Total %1 camera(s)").arg(internalModelData().size());
    case BytesColumn:
        return formatBytesString(footer.recordedBytes);
    case DurationColumn:
        return formatDurationString(footer);
    case BitrateColumn:
        return formatBitrateString(footer.bitrateSum);
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

qreal QnRecordingStatsModel::chartData(const QModelIndex &index, bool isForecast) const
{
    if (m_forecastData.size() != m_data.size() && isForecast)
        return 0.0;
    bool useForecast = !m_forecastData.isEmpty() && index.column() != BitrateColumn;
    const QnRecordingStatsReply& data = isForecast && useForecast ? m_forecastData : m_data;
    const QnCamRecordingStatsData& value = data.at(index.row());
    qreal result = 0.0;
    const auto& footer = useForecast ? m_forecastFooter : m_footer;
    switch(index.column())
    {
    case BytesColumn:
        result = value.recordedBytes / (qreal) footer.recordedBytes;
        break;
    case DurationColumn:
        result = value.archiveDurationSecs / (qreal) footer.archiveDurationSecs;
        break;
    case BitrateColumn:
        if (footer.averageBitrate > 0)
            result =  value.averageBitrate / (qreal) footer.averageBitrate;
        else
            result =  0.0;
        break;
    }
    Q_ASSERT(qBetween(0.0, result, 1.00001));
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
        return QVariant::fromValue<QnCamRecordingStatsData>(internalFooterData());
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
            return tr("%1 with non-empty archive").arg(getDefaultDevicesName());
        case BytesColumn:
            return tr("Storage space occupied by %1").arg(getDefaultDevicesName());
        case DurationColumn:
            return tr("Archived duration in calendar days between the first record and the current moment");
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

    if (index.row() >= internalModelData().size())
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
        return chartData(index, false);
    case Qn::RecordingStatForecastDataRole:
        return chartData(index, true);
    case Qn::RecordingStatColorsDataRole:
        return QVariant::fromValue<QnRecordingStatsColors>(m_colors);
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
        case CameraNameColumn: return tr("Camera");
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
    m_forecastData.clear();
    m_forecastFooter = QnFooterData();
    m_data = data;
    m_footer = calculateFooter(data);
    endResetModel();
}

void QnRecordingStatsModel::setForecastData(const QnRecordingStatsReply& data) {
    beginResetModel();
    m_forecastData = data;
    m_forecastFooter = calculateFooter(data);
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

const QnRecordingStatsReply& QnRecordingStatsModel::internalModelData() const {
    return m_forecastData.isEmpty()
        ? m_data 
        : m_forecastData;
}

const QnFooterData& QnRecordingStatsModel::internalFooterData() const {
    return m_forecastData.isEmpty()
        ? m_footer 
        : m_forecastFooter;
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


QString QnRecordingStatsModel::formatDurationString(const QnCamRecordingStatsData &data) const {
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
