#include "ptz_manage_model.h"

#include <common/common_globals.h>

#include <ui/style/globals.h>
#include <utils/common/container.h>

QnPtzManageModel::QnPtzManageModel(QObject *parent) :
    base_type(parent)
{
}

QnPtzManageModel::~QnPtzManageModel() {
}

const QList<QnPtzTourItemModel>& QnPtzManageModel::tourModels() const {
    return m_tours;
}

const QStringList& QnPtzManageModel::removedTours() const {
    return m_removedTours;
}

void QnPtzManageModel::setTours(const QnPtzTourList &tours) {
    beginResetModel();
    m_tours.clear();
    foreach (const QnPtzTour &tour, tours)
        m_tours << tour;
    endResetModel();
}

void QnPtzManageModel::addTour() {
    int firstRow = rowCount();
    int lastRow = firstRow;
    if (m_tours.isEmpty())
        lastRow++;

    beginInsertRows(QModelIndex(), firstRow, lastRow);
    m_tours << tr("New Tour %1").arg(m_tours.size() + 1);
    endInsertRows();
}

void QnPtzManageModel::removeTour(const QString &id) {
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

const QStringList & QnPtzManageModel::removedPresets() const {
    return m_removedPresets;
}

const QList<QnPtzPresetItemModel> & QnPtzManageModel::presetModels() const {
    return m_presets;
}


const QnPtzPresetList& QnPtzManageModel::presets() const {
    return m_ptzPresetsCache;
}

void QnPtzManageModel::setPresets(const QnPtzPresetList &presets) {
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

void QnPtzManageModel::addPreset() {
    int firstRow = m_presets.size();
    if (firstRow > 0)
        firstRow++;  //space for caption

    int lastRow = firstRow;
    if (m_presets.isEmpty())
        lastRow++;

    beginInsertRows(QModelIndex(), firstRow, lastRow);
    m_presets << tr("Saved Position %1").arg(m_presets.size() + 1);
    endInsertRows();

    updatePresetsCache();
}

void QnPtzManageModel::removePreset(const QString &id) {
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

void QnPtzManageModel::setHotkeys(const QnPtzHotkeyHash &hotkeys) {
    if (m_hotkeys == hotkeys)
        return;
    m_hotkeys = hotkeys;
    emit dataChanged(index(0, HotkeyColumn), index(rowCount(), HotkeyColumn));
}

const QnPtzHotkeyHash& QnPtzManageModel::hotkeys() const {
    return m_hotkeys;
}

const QString QnPtzManageModel::homePosition() const {
    return m_homePosition;
}

void QnPtzManageModel::setHomePosition(const QString &homePosition) {
    if (m_homePosition == homePosition)
        return;
    m_homePosition = homePosition;
    emit dataChanged(index(0, HomeColumn), index(rowCount(), HomeColumn));
}

int QnPtzManageModel::rowCount(const QModelIndex &parent) const {
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

int QnPtzManageModel::columnCount(const QModelIndex &parent) const {
    if(parent.isValid())
        return 0;
    return ColumnCount;
}

bool QnPtzManageModel::removeRows(int row, int count, const QModelIndex &parent) {
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

QVariant QnPtzManageModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.model() != this || !hasIndex(index.row(), index.column(), index.parent()))
        return QVariant();
    
    RowData data = rowData(index.row());

    switch (data.rowType) {
    case QnPtzManageModel::PresetTitleRow:
    case QnPtzManageModel::TourTitleRow:
        return titleData(data.rowType, index.column(), role);
    case QnPtzManageModel::PresetRow:
        return presetData(data.presetModel, index.column(), role);
    case QnPtzManageModel::TourRow:
        return tourData(data.tourModel, index.column(), role);
    default:
        break;
    }
    return QVariant();
}

bool QnPtzManageModel::setData(const QModelIndex &index, const QVariant &value, int role) {
    if (!index.isValid())
        return false;

    RowData data = rowData(index.row());

    if (role == Qt::CheckStateRole && index.column() == HomeColumn) {
        if (value.toInt() == Qt::Checked) {
            switch (data.rowType) {
            case QnPtzManageModel::PresetRow:
                m_homePosition = data.presetModel.preset.id;
                break;
            case QnPtzManageModel::TourRow:
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

    } else if (role == Qt::EditRole && index.column() == HotkeyColumn) {

        bool ok = false;
        int hotkey = value.toInt(&ok);
        if(!ok || hotkey > 9)
            return false;

        QString id;
        switch (data.rowType) {
        case QnPtzManageModel::PresetRow:
            id = data.presetModel.preset.id;
            break;
        case QnPtzManageModel::TourRow:
            id = data.tourModel.tour.id;
            break;
        default:
            return false;
        }

        // hotkey that was assigned to this preset
        int oldHotkey = m_hotkeys.key(id, QnPtzHotkey::NoHotkey);
        if (oldHotkey == hotkey)
            return false;

        // preset that is assigned to this hotkey
        QString existing;
        if (hotkey != QnPtzHotkey::NoHotkey)
            existing = m_hotkeys[hotkey];

        // set old hotkey to an existing preset (or empty)
        if (oldHotkey != QnPtzHotkey::NoHotkey)
            m_hotkeys[oldHotkey] = existing;

        // set updated hotkey
        if (hotkey != QnPtzHotkey::NoHotkey)
            m_hotkeys[hotkey] = id;

        //TODO: update only affected rows? low priority
        //TODO: do not clear existing hotkey, highlight duplicated instead and do not allow to save them
        emit dataChanged(index.sibling(0, HotkeyColumn), index.sibling(rowCount() - 1, HotkeyColumn)); 
        return true;
    } else if (role == Qt::EditRole && index.column() == NameColumn) {
        QString newName = value.toString().trimmed();
        if (newName.isEmpty())
            return false;

        switch (data.rowType) {
        case QnPtzManageModel::PresetRow: 
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
        case QnPtzManageModel::TourRow:
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

QVariant QnPtzManageModel::headerData(int section, Qt::Orientation orientation, int role) const {
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

Qt::ItemFlags QnPtzManageModel::flags(const QModelIndex &index) const {
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
        flags |= Qt::ItemIsUserCheckable;
        break;
    default:
        break;
    }

    return flags;
}

void QnPtzManageModel::updateTourSpots(const QString tourId, const QnPtzTourSpotList &spots) {
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

qint64 QnPtzManageModel::estimatedTimeSecs(const QnPtzTour &tour) const {
    qint64 result = 0;
    foreach (const QnPtzTourSpot &spot, tour.spots)
        result += spot.stayTime;
    return result / 1000;
}

QnPtzManageModel::RowData QnPtzManageModel::rowData(int row) const {
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

QVariant QnPtzManageModel::titleData(RowType rowType,  int column, int role) const {
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
        return QColor(Qt::lightGray);       //TODO: skin
    case Qt::ForegroundRole:
        return QColor(Qt::black);           //TODO: skin
    case Qt::FontRole: 
        {
            QFont f;
            f.setBold(true);
            return f;
        }
        
    default:
        break;
    }

    return QVariant();
}

QVariant QnPtzManageModel::presetData(const QnPtzPresetItemModel &presetModel, int column, int role) const {
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
            {
                int hotkey = m_hotkeys.key(presetModel.preset.id, QnPtzHotkey::NoHotkey);
                return hotkey < 0 ? tr("None") : QString::number(hotkey);
            }
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

QVariant QnPtzManageModel::tourData(const QnPtzTourItemModel &tourModel, int column,  int role) const {
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
            {
                int hotkey = m_hotkeys.key(tourModel.tour.id, QnPtzHotkey::NoHotkey);
                return hotkey < 0 ? tr("None") : QString::number(hotkey);
            }
        case DetailsColumn:
            if (tourIsValid(tourModel)) {
                qint64 time = estimatedTimeSecs(tourModel.tour);
                return tr("Tour time: %1").arg((time < 60) ? tr("less than a minute") : tr("about %n minutes", 0, time / 60));
            }
                //TODO: more detailed message required: 
            // - tour require at least two different positions
            // - there shouldn't be two same positions in a row
            // - etc.
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
        //TODO: some gradations required: fully invalid, only warning (eg. hotkey duplicates)
        return tourIsValid(tourModel);
    default:
        break;
    }
    return QVariant();
}

bool QnPtzManageModel::tourIsValid(const QnPtzTourItemModel &tourModel) const {
    return tourModel.tour.isValid(m_ptzPresetsCache);
}

void QnPtzManageModel::updatePresetsCache() {
    m_ptzPresetsCache.clear();
    foreach (const QnPtzPresetItemModel &model, m_presets) {
        m_ptzPresetsCache << model.preset;
    }
    emit presetsChanged(m_ptzPresetsCache);
}

bool QnPtzManageModel::synchronized() const {
    foreach (const QnPtzPresetItemModel &model, m_presets) {
        if (model.local || model.modified)
            return false;
    }

    foreach (const QnPtzTourItemModel &model, m_tours) {
        if (model.local || model.modified)
            return false;
    }

    return true;
}

Q_SLOT void QnPtzManageModel::setSynchronized() {
    beginResetModel();
    auto presetIter = m_presets.begin();
    while (presetIter != m_presets.end()) {
        presetIter->local = false;
        presetIter->modified = false;
        presetIter++;
    }

    auto tourIter = m_tours.begin();
    while (tourIter != m_tours.end()) {
        tourIter->local = false;
        tourIter->modified = false;
        tourIter++;
    }
    endResetModel();

    m_removedPresets.clear();
    m_removedTours.clear();
}
