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

const QStringList& QnPtzTourListModel::removedTours() const {
    return m_removedTours;
}

void QnPtzTourListModel::setTours(const QnPtzTourList &tours) {
    beginResetModel();
    m_tours.clear();
    foreach (const QnPtzTour &tour, tours)
        m_tours.append(QnPtzTourItemModel(tour));
    endResetModel();
}

void QnPtzTourListModel::addTour() {
    int firstRow = rowCount();
    int lastRow = firstRow;
    if (m_tours.isEmpty())
        lastRow++;

    beginInsertRows(QModelIndex(), firstRow, lastRow);
    m_tours << QnPtzTourItemModel(tr("New Tour %1").arg(m_tours.size() + 1));
    endInsertRows();
}

const QnPtzPresetList& QnPtzTourListModel::presets() const {
    return m_presets;
}

void QnPtzTourListModel::setPresets(const QnPtzPresetList &presets) {
    if (m_presets == presets)
        return;

    beginResetModel();
    m_presets = presets;
    endResetModel();

    emit presetsChanged(m_presets);
}

void QnPtzTourListModel::addPreset() {
    int firstRow = m_presets.size();
    if (firstRow > 0)
        firstRow++;  //space for caption

    int lastRow = firstRow;
    if (m_presets.isEmpty())
        lastRow++;

    beginInsertRows(QModelIndex(), firstRow, lastRow);
    m_presets << QnPtzPreset(QUuid::createUuid().toString(), tr("Position %1").arg(m_presets.size()));
    endInsertRows();
}

void QnPtzTourListModel::setHotkeys(const QnPtzHotkeyHash &hotkeys) {
    m_hotkeys = hotkeys;
    //TODO: compare and update model
}

const QnPtzHotkeyHash& QnPtzTourListModel::hotkeys() const {
    return m_hotkeys;
}


int QnPtzTourListModel::rowCount(const QModelIndex &parent) const {
    if(parent.isValid())
        return 0;

    int presets = m_presets.size();
    if (presets > 0)
        presets++;  //space for caption

    int tours = m_tours.size();
    if (tours > 0)
        tours++;   //space for caption

    return presets + tours;
}

int QnPtzTourListModel::columnCount(const QModelIndex &parent) const {
    if(parent.isValid())
        return 0;
    return ColumnCount;
}

bool QnPtzTourListModel::removeRows(int row, int count, const QModelIndex &parent) {
    if(parent.isValid() || row < 0 || count < 1 || row + count > m_tours.size())
        return false;

    auto iter = m_tours.begin() + row;
    while(iter < m_tours.begin() + row + count) {
        if (!iter->tour.id.isEmpty())
            m_removedTours << iter->tour.id;
        iter++;
    }

    beginRemoveRows(parent, row, row + count - 1);
    m_tours.erase(m_tours.begin() + row, m_tours.begin() + row + count);
    endRemoveRows();
    return true;
}

QVariant QnPtzTourListModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.model() != this || !hasIndex(index.row(), index.column(), index.parent()))
        return QVariant();
    
    Row row = rowClass(index.row());

    switch (row) {
    case QnPtzTourListModel::PresetTitleRow:
    case QnPtzTourListModel::TourTitleRow:
        return titleData(row, role);
    case QnPtzTourListModel::PresetRow:
        return presetData(index.row(), index.column(), role);
    case QnPtzTourListModel::TourRow:
        return tourData(index.row(), index.column(), role);
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

    if (value.toString().trimmed().isEmpty())
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
        case HotkeyColumn:
            return tr("Hotkey");
        case HomeColumn:
            return tr("Home");
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
    if (!index.isValid()) 
        return flags;

    Row row = rowClass(index.row());
    if (row == InvalidRow || row == PresetTitleRow || row == TourTitleRow)
        return flags;

    switch (index.column()) {
    case NameColumn:
    case HotkeyColumn:
        flags |= Qt::ItemIsEditable;
        break;
    case HomeColumn:
        flags |= Qt::ItemIsEditable | Qt::ItemIsUserCheckable;
        break;
    default:
        break;
    }

    return flags;
}

void QnPtzTourListModel::updateTourSpots(const QString tourId, const QnPtzTourSpotList &spots) {
    for (int i = 0; i < m_tours.size(); ++i) {
        if (m_tours[i].tour.id != tourId)
            continue;
        if (m_tours[i].tour.spots == spots)
            return; //no changes were made

        m_tours[i].tour.spots = spots;
        m_tours[i].modified = true;
        emit dataChanged(index(i, 0), index(i, ColumnCount));
        break;
    }

}

qint64 QnPtzTourListModel::estimatedTimeSecs(const QnPtzTour &tour) const {
    qint64 result = 0;
    foreach (const QnPtzTourSpot &spot, tour.spots)
        result += spot.stayTime;
    return result / 1000;
}



QnPtzTourListModel::Row QnPtzTourListModel::rowClass(int row) const {
    int presetOffset = 0;
    if (m_presets.size() > 0) {
        if (row == 0)
            return PresetTitleRow;
        if (row <= m_presets.size())
            return PresetRow;
        presetOffset = m_presets.size() + 1;
    }

    if (m_tours.size() > 0) {
        if (row == presetOffset)
            return TourTitleRow;
        if (row - presetOffset <= m_tours.size() + 1)
            return TourRow;
    }

    assert(false); //TODO: remove assert in production
    return InvalidRow;

}

QVariant QnPtzTourListModel::titleData(Row row, int role) const {
    switch(role) {
    case Qt::DisplayRole:
    case Qt::ToolTipRole:
    case Qt::StatusTipRole:
    case Qt::WhatsThisRole:
    case Qt::AccessibleTextRole:
    case Qt::AccessibleDescriptionRole:
        if (row == TourTitleRow)
            return tr("Tours");
        return tr("Saved Positions");

    case Qt::BackgroundRole:
        return QColor(Qt::gray);    //TODO: style
    default:
        break;
    }

    return QVariant();
}

QVariant QnPtzTourListModel::presetData(int row, int column, int role) const {
    return tr("ololo");
 /*   switch(role) {
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
    }*/

    return QVariant();
}

QVariant QnPtzTourListModel::tourData(int row, int column,  int role) const {
    return tr("ololo");
 /*   switch(role) {
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
    */
    return QVariant();
}
