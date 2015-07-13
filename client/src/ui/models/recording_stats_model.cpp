#include "recording_stats_model.h"
#include "core/resource_management/resource_pool.h"
#include <ui/common/ui_resource_name.h>
#include "ui/style/resource_icon_cache.h"

namespace {
    const qreal BYTES_IN_GB = 1000000000.0;
    const qreal SECS_PER_DAY = 3600 * 24;
    const qreal BYTES_IN_MB = 1000000.0;
    const int PREC = 2;
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
        return leftData.recordedSecs < rightData.recordedSecs;
    case QnRecordingStatsModel::BitrateColumn:
        {
            qreal leftBitrate = 0;
            qreal rightBitrate = 0;
            if (leftData.recordedSecs > 0)
                leftBitrate = leftData.recordedBytes / (qreal) leftData.recordedSecs;
            if (rightData.recordedSecs > 0)
                rightBitrate = rightData.recordedBytes / (qreal) rightData.recordedSecs;
            return leftBitrate < rightBitrate;
        }
    default:
        return false;
    }
}

QnRecordingStatsModel::QnRecordingStatsModel(QObject *parent) :
    base_type(parent)
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
    const QnCamRecordingStatsData& value = m_data.at(index.row());
    switch(index.column())
    {
        case CameraNameColumn:
            return getResourceName(qnResPool->getResourceByUniqueId(value.uniqueId));
        case BytesColumn:
            return QString::number(value.recordedBytes  / BYTES_IN_GB, 'f', PREC) + lit(" Gb");
        case DurationColumn:
            return QString::number(value.recordedSecs / SECS_PER_DAY, 'f', PREC);
        case BitrateColumn:
            if (value.recordedSecs > 0)
                return QString::number(value.averageBitrate / BYTES_IN_MB * 8, 'f', PREC) + lit(" Mbps"); // *8 == value in bits
            else
                return lit("-");
        default:
            return QString();
    }
}

QString QnRecordingStatsModel::footerDisplayData(const QModelIndex &index) const
{
    const QnCamRecordingStatsData& value = m_data.at(index.row());
    switch(index.column())
    {
    case CameraNameColumn:
        return tr("Total:");
    case BytesColumn:
    case DurationColumn:
        return displayData(index);
    case BitrateColumn:
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
    const QnRecordingStatsReply& data = isForecast ? m_forecastData : m_data;
    const QnCamRecordingStatsData& value = data.at(index.row());
    qreal result = 0.0;
    const QnCamRecordingStatsData& footer = m_forecastData.isEmpty() ? m_data.last() : m_forecastData.last();
    switch(index.column())
    {
    case BytesColumn:
        result = value.recordedBytes / (qreal) footer.recordedBytes;
        break;
    case DurationColumn:
        result = value.recordedSecs / (qreal) footer.recordedSecs;
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
    default:
        break;
    }
    return QVariant();
}

QVariant QnRecordingStatsModel::headerData(int section, Qt::Orientation orientation, int role) const 
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole && section < ColumnCount) {
        switch(section) {
        case CameraNameColumn: return tr("Camera");
        case BytesColumn:      return tr("Space");
        case DurationColumn:   return tr("Days");
        case BitrateColumn:    return tr("Bitrate");
        default:
            break;
        }
    }
    return base_type::headerData(section, orientation, role);
}

void QnRecordingStatsModel::clear() {
    beginResetModel();
    m_data.clear();
    endResetModel();
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
    return m_data;
}

void QnRecordingStatsModel::setModelDataInternal(const QnRecordingStatsReply& data, QnRecordingStatsReply& result)
{
    beginResetModel();
    result = data;

    QnRecordingStatsData summ;
    QnRecordingStatsData maxValue;

    for(const QnCamRecordingStatsData& value: m_data) {
        summ += value;
        maxValue.recordedBytes = qMax(maxValue.recordedBytes, value.recordedBytes);
        maxValue.recordedSecs = qMax(maxValue.recordedSecs, value.recordedSecs);
        maxValue.averageBitrate = qMax(maxValue.averageBitrate, value.averageBitrate);
    }
    QnRecordingStatsData footer;
    footer.recordedBytes = summ.recordedBytes;
    footer.recordedSecs = summ.recordedSecs;
    footer.averageBitrate = maxValue.averageBitrate;
    result.push_back(std::move(footer));

    endResetModel();
}
