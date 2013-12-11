#include "ptz_tour_list_model.h"

#include <common/common_globals.h>

#include <core/ptz/ptz_tour.h>

QnPtzTourListModel::QnPtzTourListModel(QObject *parent) :
    QAbstractTableModel(parent)
{
}

QnPtzTourListModel::~QnPtzTourListModel() {
}

const QnPtzTourList& QnPtzTourListModel::tours() const {
    return m_tours;
}

void QnPtzTourListModel::setTours(const QnPtzTourList &tours) {
    beginResetModel();
    m_tours = tours;
    endResetModel();
}

int QnPtzTourListModel::rowCount(const QModelIndex &parent) const {
    if(!parent.isValid())
        return m_tours.size();
    return 0;
}

int QnPtzTourListModel::columnCount(const QModelIndex &parent) const {
    if(!parent.isValid())
        return 1;
    return 0;
}

bool QnPtzTourListModel::removeRows(int row, int count, const QModelIndex &parent) {
    if(parent.isValid() || row < 0 || count < 1 || row + count > m_tours.size())
        return false;

    beginRemoveRows(parent, row, row + count - 1);
    m_tours.erase(m_tours.begin() + row, m_tours.begin() + row + count);
    endRemoveRows();
    return true;
}

bool QnPtzTourListModel::insertRows(int row, int count, const QModelIndex &parent) {
    if(parent.isValid() || row < 0 || row > m_tours.size() || count < 1)
        return false;

    beginInsertRows(parent, row, row + count - 1);
    for(int i = 0; i < count; i++)
        m_tours.insert(row, QnPtzTour());
    endInsertRows();
    return true;
}

QVariant QnPtzTourListModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.model() != this || !hasIndex(index.row(), index.column(), index.parent()))
        return QVariant();

    const QnPtzTour &tour = m_tours[index.row()];

    switch(role) {
    case Qt::DisplayRole:
    case Qt::ToolTipRole:
    case Qt::StatusTipRole:
    case Qt::WhatsThisRole:
    case Qt::AccessibleTextRole:
    case Qt::AccessibleDescriptionRole:
    case Qt::EditRole:
        return tour.name;
    case Qn::PtzTourRole:
        return QVariant::fromValue<QnPtzTour>(tour);
    default:
        break;
    }

    return QVariant();
}


QVariant QnPtzTourListModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
        return tr("Name");
    return base_type::headerData(section, orientation, role);
}
