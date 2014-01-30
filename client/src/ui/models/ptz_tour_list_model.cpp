#include "ptz_tour_list_model.h"

#include <common/common_globals.h>

#include <core/ptz/ptz_preset.h>

#include <ui/style/globals.h>

QnPtzTourListModel::QnPtzTourListModel(QObject *parent) :
    base_type(parent)
{
}

QnPtzTourListModel::~QnPtzTourListModel() {
}

const QList<QnPtzTourItemModel>& QnPtzTourListModel::tourModels() const {
    return m_tours;
}

void QnPtzTourListModel::setTours(const QnPtzTourList &tours) {
    beginResetModel();
    m_tours.clear();
    foreach (const QnPtzTour &tour, tours)
        m_tours.append(QnPtzTourItemModel(tour));
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
        m_tours.insert(row, QnPtzTourItemModel(tr("New Tour %1").arg(m_tours.size() + 1)));
    endInsertRows();
    return true;
}

QVariant QnPtzTourListModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.model() != this || !hasIndex(index.row(), index.column(), index.parent()))
        return QVariant();

    const QnPtzTourItemModel &model = m_tours[index.row()];

    switch(role) {
    case Qt::DisplayRole:
    case Qt::ToolTipRole:
    case Qt::StatusTipRole:
    case Qt::WhatsThisRole:
    case Qt::AccessibleTextRole:
    case Qt::AccessibleDescriptionRole:
    case Qt::EditRole:
        switch (index.column()) {
        case ModifiedColumn:
            return model.modified ? QLatin1String("*") : QString();
        case NameColumn:
            return model.tour.name;
        case DetailsColumn:
            if (model.tour.isValid(m_presets)) {
                qint64 time = estimatedTimeSecs(model.tour);
                return tr("Tour time: %1").arg((time < 60) ? tr("less than a minute") : tr("about %n minutes", 0, time / 60));
            }
            return tr("Invalid tour");
        default:
            break;
        }
        return QVariant();

    case Qt::BackgroundRole:
        if (!model.tour.isValid(m_presets))
            return QBrush(qnGlobals->businessRuleInvalidColumnBackgroundColor());
        break;
    case Qn::PtzTourRole:
        return QVariant::fromValue<QnPtzTour>(model.tour);
    case Qn::ValidRole:
        return model.tour.isValid(m_presets);
    default:
        break;
    }

    return QVariant();
}

bool QnPtzTourListModel::setData(const QModelIndex &index, const QVariant &value, int role) {
    if (!index.isValid())
        return false;

    if (role != Qt::EditRole)
        return false;

    if (index.column() != NameColumn)
        return false;

    if (index.row() < 0 || index.row() >= m_tours.size())
        return false;

    QnPtzTourItemModel &model = m_tours[index.row()];
    if (model.tour.name == value.toString())
        return false;

    model.tour.name = value.toString();
    model.modified = true;
    return true;
}

QVariant QnPtzTourListModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        switch (section) {
        case ModifiedColumn:
            return tr("#");
        case NameColumn:
            return tr("Name");
        case DetailsColumn:
            return tr("Details");
        default:
            break;
        }
    }
    return base_type::headerData(section, orientation, role);
}

Qt::ItemFlags QnPtzTourListModel::flags(const QModelIndex &index) const {
    Qt::ItemFlags flags = base_type::flags(index);
    if (index.isValid() && index.column() == NameColumn)
        flags |= Qt::ItemIsEditable;
    return flags;
}

void QnPtzTourListModel::updateTour(const QnPtzTour &tour) {
    for (int i = 0; i < m_tours.size(); ++i) {
        if (m_tours[i].tour.id != tour.id)
            continue;
        if (m_tours[i].tour == tour)
            return; //no changes were made

        m_tours[i].tour = tour;
        m_tours[i].modified = true;
        emit dataChanged(index(i, 0), index(i, ColumnCount));
        break;
    }

}

qint64 QnPtzTourListModel::estimatedTimeSecs(const QnPtzTour &tour) const {
    qint64 result = 0;
    foreach (const QnPtzTourSpot spot, tour.spots)
        result += spot.stayTime;
    return result;
}
