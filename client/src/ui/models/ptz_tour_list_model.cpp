#include "ptz_tour_list_model.h"

#include <common/common_globals.h>

#include <ui/style/globals.h>
#include <utils/common/container.h>

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
        m_tours << tour;
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

void QnPtzTourListModel::removeTour(const QString &id) {
    int idx = qnIndexOf(m_tours, [&](const QnPtzTourItemModel &model) { return model.tour.id == id; });
    if (idx < 0)
        return;

    int offset = m_presets.size();
    if (offset > 0)
        offset++;   //presets title
    
    int firstRow = offset + idx;
    if (m_tours.size() > 1)
        firstRow++; // do not remove tour title
    int lastRow = offset + idx + 1; // +1 for tour title

    beginRemoveRows(QModelIndex(), firstRow, lastRow);
    m_removedTours << m_tours.takeAt(idx).tour.id;
    endRemoveRows();
}

const QnPtzPresetList& QnPtzTourListModel::presets() const {
    return m_ptzPresetsCache;
}

void QnPtzTourListModel::setPresets(const QnPtzPresetList &presets) {
    if (m_ptzPresetsCache == presets)
        return;

    beginResetModel();
    m_presets.clear();
    m_ptzPresetsCache.clear();
    foreach (const QnPtzPreset &preset, presets)
        m_presets << preset;
    endResetModel();

    updatePresetsCache();
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

    updatePresetsCache();
}

void QnPtzTourListModel::removePreset(const QString &id) {
    int idx = qnIndexOf(m_presets, [&](const QnPtzPresetItemModel &model) { return model.preset.id == id; });
    if (idx < 0)
        return;

    int firstRow = idx;
    if (m_presets.size() > 1)
        firstRow++; // do not remove presets title
    int lastRow = idx + 1; // +1 for presets title

    beginRemoveRows(QModelIndex(), firstRow, lastRow);
    m_removedPresets << m_presets.takeAt(idx).preset.id;
    endRemoveRows();

    updatePresetsCache();
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
    
    RowData data = rowData(index.row());

    switch (data.rowType) {
    case QnPtzTourListModel::PresetTitleRow:
    case QnPtzTourListModel::TourTitleRow:
        return titleData(data.rowType, index.column(), role);
    case QnPtzTourListModel::PresetRow:
        return presetData(data.presetModel, index.column(), role);
    case QnPtzTourListModel::TourRow:
        return tourData(data.tourModel, index.column(), role);
    default:
        break;
    }
    return QVariant();
}

bool QnPtzTourListModel::setData(const QModelIndex &index, const QVariant &value, int role) {
    if (!index.isValid())
        return false;

    RowData data = rowData(index.row());

    if (role == Qt::CheckStateRole && index.column() == HomeColumn) {
        if (value.toInt() == Qt::Checked) {
            switch (data.rowType) {
            case QnPtzTourListModel::PresetRow:
                m_homePosition = data.presetModel.preset.id;
                break;
            case QnPtzTourListModel::TourRow:
                m_homePosition = data.tourModel.tour.id;
                break;
            default:
                return false;
            }
        } else {
            m_homePosition = QString();
        }

        emit dataChanged(index.sibling(0, HomeColumn), index.sibling(rowCount() - 1, HomeColumn));
        return true;
    } 
    
    if (role == Qt::EditRole && index.column() == NameColumn) {
        QString newName = value.toString().trimmed();
        if (newName.isEmpty())
            return false;

        switch (data.rowType) {
        case QnPtzTourListModel::PresetRow: 
            {
                int idx = qnIndexOf(m_presets, [&](const QnPtzPresetItemModel &model) {return data.presetModel.preset.id == model.preset.id; });
                if (idx < 0)
                    return false;
                QnPtzPresetItemModel &model = m_presets[idx];
                if (model.preset.name == newName)
                    return false;
                model.preset.name = value.toString();
                model.modified = true;
                return true;
            }
        case QnPtzTourListModel::TourRow:
            {
                int idx = qnIndexOf(m_tours, [&](const QnPtzTourItemModel &model) {return data.tourModel.tour.id == model.tour.id; });
                if (idx < 0)
                    return false;
                QnPtzTourItemModel &model = m_tours[idx];
                if (model.tour.name == newName)
                    return false;
                model.tour.name = value.toString();
                model.modified = true;
                return true;
            }
        default:
            return false;
        }
    }

    return false;
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
    Qt::ItemFlags flags = Qt::ItemNeverHasChildren;

    if (!index.isValid()) 
        return flags;

    flags |= Qt::ItemIsEnabled;

    RowData data = rowData(index.row());

    if (data.rowType == InvalidRow 
        || data.rowType == PresetTitleRow 
        || data.rowType == TourTitleRow)
        return flags;

    flags |= Qt::ItemIsSelectable;

    switch (index.column()) {
    case NameColumn:
    case HotkeyColumn:
        flags |= Qt::ItemIsEditable;
        break;
    case HomeColumn:
        flags |= Qt::ItemIsUserCheckable | Qt::ItemIsEditable;
        break;
    default:
        break;
    }

    return flags;
}

void QnPtzTourListModel::updateTourSpots(const QString tourId, const QnPtzTourSpotList &spots) {
    int idx = qnIndexOf(m_tours, [&](const QnPtzTourItemModel &model) { return model.tour.id == tourId; });
    if (idx < 0)
        return;

    if (m_tours[idx].tour.spots == spots)
        return; //no changes were made

    m_tours[idx].tour.spots = spots;
    m_tours[idx].modified = true;

    int row = m_presets.isEmpty() ? idx + 1 : m_presets.size() + idx + 2;
    emit dataChanged(index(row, 0), index(row, ColumnCount));
}

qint64 QnPtzTourListModel::estimatedTimeSecs(const QnPtzTour &tour) const {
    qint64 result = 0;
    foreach (const QnPtzTourSpot &spot, tour.spots)
        result += spot.stayTime;
    return result / 1000;
}

QnPtzTourListModel::RowData QnPtzTourListModel::rowData(int row) const {
    RowData result;

    int presetOffset = 0;
    if (m_presets.size() > 0) {
        if (row == 0) {
            result.rowType = PresetTitleRow;
            return result;
        }

        if (row <= m_presets.size()) {
            result.rowType = PresetRow;
            result.presetModel = m_presets[row - 1];
            return result;
        }
        presetOffset = m_presets.size() + 1;
    }

    if (m_tours.size() > 0) {
        if (row == presetOffset) {
            result.rowType = TourTitleRow;
            return result;
        }

        if (row - presetOffset <= m_tours.size() + 1) {
            result.rowType = TourRow;
            result.tourModel = m_tours[row - presetOffset - 1];
            return result;
        }
    }

    return result;
}

QVariant QnPtzTourListModel::titleData(RowType rowType,  int column, int role) const {
    switch(role) {
    case Qt::DisplayRole:
    case Qt::ToolTipRole:
    case Qt::StatusTipRole:
    case Qt::WhatsThisRole:
    case Qt::AccessibleTextRole:
    case Qt::AccessibleDescriptionRole:
        if (column != NameColumn) 
            return QVariant();

        if (rowType == TourTitleRow)
            return tr("Tours");
        return tr("Positions");

    case Qt::BackgroundRole:
        return QColor(Qt::lightGray);    //TODO: style
    case Qt::ForegroundRole:
        return QColor(Qt::black);
    default:
        break;
    }

    return QVariant();
}

QVariant QnPtzTourListModel::presetData(const QnPtzPresetItemModel &presetModel, int column, int role) const {
    switch(role) {
    case Qt::DisplayRole:
    case Qt::ToolTipRole:
    case Qt::StatusTipRole:
    case Qt::WhatsThisRole:
    case Qt::AccessibleTextRole:
    case Qt::AccessibleDescriptionRole:
    case Qt::EditRole:
        switch (column) {
        case ModifiedColumn:
            return presetModel.modified ? QLatin1String("*") : QString();
        case NameColumn:
            return presetModel.preset.name;
        case HotkeyColumn:
            return tr("0");
        case DetailsColumn:
            return QVariant();
        default:
            break;
        }
        return QVariant();
    case Qt::CheckStateRole:
        if (column == HomeColumn)
            return m_homePosition == presetModel.preset.id 
            ? Qt::Checked 
            : Qt::Unchecked;
        break;
    case Qn::PtzPresetRole:
        return QVariant::fromValue<QnPtzPreset>(presetModel.preset);
    case Qn::ValidRole:
        return true;
    default:
        break;
    }

    return QVariant();
}

QVariant QnPtzTourListModel::tourData(const QnPtzTourItemModel &tourModel, int column,  int role) const {
    switch(role) {
    case Qt::DisplayRole:
    case Qt::ToolTipRole:
    case Qt::StatusTipRole:
    case Qt::WhatsThisRole:
    case Qt::AccessibleTextRole:
    case Qt::AccessibleDescriptionRole:
    case Qt::EditRole:
        switch (column) {
        case ModifiedColumn:
            return tourModel.modified ? QLatin1String("*") : QString();
        case NameColumn:
            return tourModel.tour.name;
        case HotkeyColumn:
            return tr("0");
        case DetailsColumn:
            if (tourIsValid(tourModel)) {
                qint64 time = estimatedTimeSecs(tourModel.tour);
                return tr("Tour time: %1").arg((time < 60) ? tr("less than a minute") : tr("about %n minutes", 0, time / 60));
            }
            return tr("Invalid tour");
        default:
            break;
        }
        return QVariant();

    case Qt::BackgroundRole:
        if (!tourIsValid(tourModel))
            return QBrush(qnGlobals->businessRuleInvalidColumnBackgroundColor());
        break;
    case Qt::CheckStateRole:
        if (column == HomeColumn)
            return m_homePosition == tourModel.tour.id;
        break;
    case Qn::PtzTourRole:
        return QVariant::fromValue<QnPtzTour>(tourModel.tour);
    case Qn::ValidRole:
        return tourIsValid(tourModel);
    default:
        break;
    }
    return QVariant();
}

bool QnPtzTourListModel::tourIsValid(const QnPtzTourItemModel &tourModel) const {
    return tourModel.tour.isValid(m_ptzPresetsCache);
}

void QnPtzTourListModel::updatePresetsCache() {
    m_ptzPresetsCache.clear();
    foreach (const QnPtzPresetItemModel &model, m_presets) {
        m_ptzPresetsCache << model.preset;
    }
    emit presetsChanged(m_ptzPresetsCache);
}
