#include "recording_stats_model.h"

QnRecordingStatsModel::QnRecordingStatsModel(QObject *parent) :
    base_type(parent)
    //QnWorkbenchContextAware(parent)
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
    return (int) m_data.size();
}

int QnRecordingStatsModel::columnCount(const QModelIndex &parent) const {
    Q_UNUSED(parent);
    return ColumnCount;
}

QString QnRecordingStatsModel::textData(const QModelIndex &index) const
{
    const QnRecordingStatsData& value = m_data.at(index.row());
    return QString();
    /*
    switch(index.column())
    {
    case CameraNameColumn:
        return value.id;
    case TypeColumn:
        return QnLexical::serialized(value.portType);
    case DefaultStateColumn:
        return QnLexical::serialized<Qn::IODefaultState>(value.getDefaultState());
    case NameColumn:
        return value.getName();
    case AutoResetColumn:
        return QString::number(value.autoResetTimeoutMs);
    default:
        return QString();
    }
    */
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
        return QVariant(textData(index));
    case Qt::TextColorRole:
        break;
    case Qt::BackgroundRole:
        break;
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
    beginResetModel();
    m_data = data;
    endResetModel();
}
