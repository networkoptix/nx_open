#include "recording_stats_model.h"
#include "core/resource_management/resource_pool.h"
#include <ui/common/ui_resource_name.h>
#include "ui/style/resource_icon_cache.h"

namespace {
    const qreal BYTES_IN_GB = 1000000000.0;
    const qreal BYTES_IN_TB = 1000.0 * BYTES_IN_GB;
    const qreal SECS_PER_HOUR = 3600.0;
    const qreal SECS_PER_DAY = SECS_PER_HOUR * 24;
    const qreal SECS_PER_YEAR = SECS_PER_DAY * 365.0;
    const qreal SECS_PER_MONTH = SECS_PER_YEAR / 12.0;
    const qreal BYTES_IN_MB = 1000000.0;
    const int PREC = 2;

    QString formatBitrateString(qint64 bitrate)
    {
        return QString::number(bitrate / BYTES_IN_MB * 8, 'f', PREC) + lit(" Mbps"); // *8 == value in bits
    }
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

    if (left.column() == QnRecordingStatsModel::CameraNameColumn)
        return left.data(Qt::DisplayRole).toString() < right.data(Qt::DisplayRole).toString();


    switch(left.column())
    {
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
    base_type(parent),
    m_bitrateSum(0)
{
}

QnRecordingStatsModel::~QnRecordingStatsModel() {

}

QModelIndex QnRecordingStatsModel::index(int row, int column, const QModelIndex &parent) const {
    return hasIndex(row, column, parent) ? createIndex(row, column, (void*)0) : QModelIndex();
}

QModelIndex QnRecordingStatsModel::parent(const QModelIndex &child) const {
    Q_UNUSED(child)
    return QModelIndex();
}

int QnRecordingStatsModel::rowCount(const QModelIndex &parent) const {
    Q_UNUSED(parent)
    return m_data.size();
}

int QnRecordingStatsModel::columnCount(const QModelIndex &parent) const {
    Q_UNUSED(parent);
    return ColumnCount;
}

QString QnRecordingStatsModel::displayData(const QModelIndex &index) const
{
    const QnRecordingStatsReply& data = m_forecastData.isEmpty() ? m_data : m_forecastData;
    const QnCamRecordingStatsData& value = data.at(index.row());
    switch(index.column())
    {
        case CameraNameColumn:
            return getResourceName(qnResPool->getResourceByUniqueId(value.uniqueId));
        case BytesColumn:
            if (value.recordedBytes > BYTES_IN_TB)
                return QString::number(value.recordedBytes  / BYTES_IN_TB, 'f', PREC) + lit(" Tb");
            else
                return QString::number(value.recordedBytes  / BYTES_IN_GB, 'f', PREC) + lit(" Gb");
        case DurationColumn:
        {
            qint64 tmpVal = value.archiveDurationSecs;
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
        case BitrateColumn:
            if (value.averageBitrate > 0)
                return formatBitrateString(value.averageBitrate);
            else
                return lit("-");
        default:
            return QString();
    }
}

QString QnRecordingStatsModel::footerDisplayData(const QModelIndex &index) const
{
    switch(index.column())
    {
    case CameraNameColumn:
        return tr("Total %1 camera(s)").arg(m_data.size()-1);
    case BytesColumn:
    case DurationColumn:
        return displayData(index);
    case BitrateColumn:
        if (m_bitrateSum > 0)
            return formatBitrateString(m_bitrateSum);
        break;
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
    const QnCamRecordingStatsData& footer = useForecast ? m_forecastData.last() : m_data.last();
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
    case Qt::FontRole: {
        QFont font;
        font.setBold(true);
        return font;
    }
    case Qt::BackgroundRole:
        return qApp->palette().color(QPalette::Normal, QPalette::Background);
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
            return tr("Cameras with non empty archive");
        case BytesColumn:
            return tr("Storage space occupied by camera");
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
    if (index.row() >= m_data.size())
        return false;

    if (index.row() == m_data.size() - 1)
        return footerData(index, role);

    switch(role) {
    case Qt::DisplayRole:
    case Qn::DisplayHtmlRole:
        return displayData(index);
    case Qt::DecorationRole:
        return qnResIconCache->icon(getResource(index));
    case Qn::ResourceRole:
        return QVariant::fromValue<QnResourcePtr>(getResource(index));
    case Qt::TextColorRole:
        break;
    case Qt::BackgroundRole:
        return qApp->palette().color(QPalette::Normal, QPalette::Base);
        break;
    case Qn::RecordingStatsDataRole:
        return QVariant::fromValue<QnCamRecordingStatsData>(m_data.at(index.row()));
    case Qn::RecordingStatChartDataRole:
        return chartData(index, false);
    case Qn::RecordingStatForecastDataRole:
        return chartData(index, true);
    case Qn::RecordingStatColorsDataRole:
        return QVariant::fromValue<QnRecordingStatsColors>(m_colors);
    case Qt::ToolTipRole:
        return tooltipText((Columns) index.column());
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
    QN_SCOPED_MODEL_RESET(this);
    m_data.clear();
}

void QnRecordingStatsModel::setModelData(const QnRecordingStatsReply& data)
{
    setModelDataInternal(data, m_data);
}

void QnRecordingStatsModel::setForecastData(const QnRecordingStatsReply& data)
{
    setModelDataInternal(data, m_forecastData);
}

QnRecordingStatsReply QnRecordingStatsModel::modelData() const
{
    QnRecordingStatsReply result = m_data;
    if (!result.isEmpty())
        result.remove(result.size()-1); // remove footer
    return result;
}

void QnRecordingStatsModel::setModelDataInternal(const QnRecordingStatsReply& data, QnRecordingStatsReply& result)
{
    QN_SCOPED_MODEL_RESET(this);
    result = data;
    if (result.isEmpty())
        return;

    QnRecordingStatsData summ;
    QnRecordingStatsData maxValue;
    m_bitrateSum = 0;

    for(const QnCamRecordingStatsData& value: result) {
        summ += value;
        maxValue.recordedBytes = qMax(maxValue.recordedBytes, value.recordedBytes);
        maxValue.recordedSecs = qMax(maxValue.recordedSecs, value.recordedSecs);
        maxValue.archiveDurationSecs = qMax(maxValue.archiveDurationSecs, value.archiveDurationSecs);
        maxValue.averageBitrate = qMax(maxValue.averageBitrate, value.averageBitrate);
        m_bitrateSum += value.averageBitrate;
    }
    QnRecordingStatsData footer;
    footer.recordedBytes = summ.recordedBytes;
    footer.recordedSecs = summ.recordedSecs;
    footer.archiveDurationSecs = summ.archiveDurationSecs;
    footer.averageBitrate = maxValue.averageBitrate;
    result.push_back(std::move(footer)); // add footer
}

QnRecordingStatsColors QnRecordingStatsModel::colors() const
{
    return m_colors;
}

void QnRecordingStatsModel::setColors(const QnRecordingStatsColors &colors)
{
    m_colors = colors;
    emit colorsChanged();
}
