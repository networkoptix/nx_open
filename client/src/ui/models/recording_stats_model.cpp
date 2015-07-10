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
    if (left.column() == QnRecordingStatsModel::CameraNameColumn)
        return left.data(Qt::DisplayRole).toString() < right.data(Qt::DisplayRole).toString();

    QnCamRecordingStatsData leftData = left.data(Qn::RecordingStatsDataRole).value<QnCamRecordingStatsData>();
    QnCamRecordingStatsData rightData = right.data(Qn::RecordingStatsDataRole).value<QnCamRecordingStatsData>();

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
            return getResourceName(qnResPool->getResourceById(value.id));
        case BytesColumn:
            return QString::number(value.recordedBytes  / BYTES_IN_GB, 'f', PREC);
        case DurationColumn:
            return QString::number(value.recordedSecs / SECS_PER_DAY, 'f', PREC);
        case BitrateColumn:
            if (value.recordedSecs > 0)
                return QString::number(value.recordedBytes / (qreal) value.recordedSecs / BYTES_IN_MB * 8, 'f', PREC); // *8 == value in bits
            else
                return lit("-");
        default:
            return QString();
    }
}

QnResourcePtr QnRecordingStatsModel::getResource(const QModelIndex &index) const
{
    const QnCamRecordingStatsData& value = m_data.at(index.row());
    switch(index.column())
    {
    case CameraNameColumn:
        return qnResPool->getResourceById(value.id);
    default:
        return QnResourcePtr();
    }
}

QVariant QnRecordingStatsModel::data(const QModelIndex &index, int role) const 
{
    /* Check invalid indices. */
    if (!index.isValid() || index.model() != this || !hasIndex(index.row(), index.column(), index.parent()))
        return QVariant();
    if (index.row() >= m_data.size())
        return false;


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
        break;
    case Qn::RecordingStatsDataRole:
        return QVariant::fromValue<QnCamRecordingStatsData>(m_data.at(index.row()));
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
        case BytesColumn:      return tr("Space (Gb)");
        case DurationColumn:   return tr("Days");
        case BitrateColumn:    return tr("Bitrate (Mbit)");
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
    beginResetModel();
    m_data = data;
    endResetModel();
}
