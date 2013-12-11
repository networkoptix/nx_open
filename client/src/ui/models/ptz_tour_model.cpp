#include "ptz_tour_model.h"

#include <common/common_globals.h>

QnPtzTourModel::QnPtzTourModel(QObject *parent) :
    base_type(parent)
{
    connect(this, SIGNAL(dataChanged(QModelIndex,QModelIndex)), this, SLOT(at_dataChanged(QModelIndex,QModelIndex)));
}

QnPtzTourModel::~QnPtzTourModel() {
}

const QnPtzTour& QnPtzTourModel::tour() const {
    return m_tour;
}

void QnPtzTourModel::setTour(const QnPtzTour &tour) {
    beginResetModel();
    m_tour = tour;
    endResetModel();
}

const QString QnPtzTourModel::tourName() const {
    return m_tour.name;
}

void QnPtzTourModel::setTourName(const QString &name) {
    m_tour.name = name;
    emit tourChanged(m_tour);
}

int QnPtzTourModel::rowCount(const QModelIndex &parent) const {
    if(!parent.isValid())
        return m_tour.spots.size();
    return 0;
}

int QnPtzTourModel::columnCount(const QModelIndex &parent) const {
    if(!parent.isValid())
        return ColumnCount;
    return 0;
}

bool QnPtzTourModel::removeRows(int row, int count, const QModelIndex &parent) {
    if(parent.isValid() || row < 0 || count < 1 || row + count > m_tour.spots.size())
        return false;

    beginRemoveRows(parent, row, row + count - 1);
    m_tour.spots.erase(m_tour.spots.begin() + row, m_tour.spots.begin() + row + count);
    endRemoveRows();
    return true;
}

bool QnPtzTourModel::insertRows(int row, int count, const QModelIndex &parent) {
    if(parent.isValid() || row < 0 || row > m_tour.spots.size() || count < 1)
        return false;

    beginInsertRows(parent, row, row + count - 1);
    for(int i = 0; i < count; i++)
        m_tour.spots.insert(row, QnPtzTourSpot());
    endInsertRows();
    return true;
}

QVariant QnPtzTourModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.model() != this || !hasIndex(index.row(), index.column(), index.parent()))
        return QVariant();

    const QnPtzTourSpot &spot = m_tour.spots[index.row()];

    switch(role) {
    case Qt::DisplayRole:
    case Qt::ToolTipRole:
    case Qt::StatusTipRole:
    case Qt::WhatsThisRole:
    case Qt::AccessibleTextRole:
    case Qt::AccessibleDescriptionRole:
    case Qt::EditRole:
        switch (index.column()) {
        case NameColumn:
            return spot.presetId;
        case TimeColumn:
            return spot.stayTime;
        case SpeedColumn:
            return spot.speed;
        default:
            break;
        }
        return QVariant();
    case Qn::PtzTourSpotRole:
        return QVariant::fromValue<QnPtzTourSpot>(spot);
    default:
        break;
    }

    return QVariant();
}

QVariant QnPtzTourModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole && section >= 0 && section < ColumnCount) {
        switch (section) {
        case NameColumn:
            return tr("Position");
        case TimeColumn:
            return tr("Stay Time");
        case SpeedColumn:
            return tr("Speed");
        default:
            break;
        }
    }
    return base_type::headerData(section, orientation, role);
}

void QnPtzTourModel::at_dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight) {
    Q_UNUSED(topLeft)
    Q_UNUSED(bottomRight)
    emit tourChanged(m_tour);
}
