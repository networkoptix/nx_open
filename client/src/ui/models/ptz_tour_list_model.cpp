#include "ptz_tour_list_model.h"

#include <common/common_globals.h>

#include <core/ptz/ptz_preset.h>
#include <core/ptz/ptz_tour.h>

#include <ui/style/globals.h>

QnPtzTourListModel::QnPtzTourListModel(QObject *parent) :
    base_type(parent)
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

const QnPtzPresetList& QnPtzTourListModel::presets() const {
    return m_presets;
}

void QnPtzTourListModel::setPresets(const QnPtzPresetList &presets) {
    beginResetModel();
    m_presets = presets;
    endResetModel();
}

int QnPtzTourListModel::rowCount(const QModelIndex &parent) const {
    if(!parent.isValid())
        return m_tours.size();
    return 0;
}

int QnPtzTourListModel::columnCount(const QModelIndex &parent) const {
    if(!parent.isValid())
        return ColumnCount;
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
        m_tours.insert(row, QnPtzTour(QUuid::createUuid().toString(),
                                      tr("New Tour %1").arg(m_tours.size() + 1),
                                      QnPtzTourSpotList()));
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

    case Qt::BackgroundRole:
        if (!tour.isValid(m_presets))
            return QBrush(qnGlobals->businessRuleInvalidColumnBackgroundColor());
        break;
    case Qn::PtzTourRole:
        return QVariant::fromValue<QnPtzTour>(tour);
    case Qn::ValidRole:
        return tour.isValid(m_presets);
    default:
        break;
    }

    return QVariant();
}

QVariant QnPtzTourListModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole && section >= 0 && section < ColumnCount)
        return tr("Name");
    return base_type::headerData(section, orientation, role);
}

void QnPtzTourListModel::updateTour(const QnPtzTour &tour) {
    for (int i = 0; i < m_tours.size(); ++i) {
        if (m_tours[i].id != tour.id)
            continue;
        if (m_tours[i] == tour)
            return; //no changes were made

        m_tours[i] = tour;
        emit dataChanged(index(i, 0), index(i, ColumnCount));
        break;
    }

}
