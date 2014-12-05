#include "ptz_manage_model.h"

#include <common/common_globals.h>

#include <ui/style/globals.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <utils/common/collection.h>
#include <utils/common/string.h>

QnPtzManageModel::QnPtzManageModel(QObject *parent) :
    base_type(parent),
    m_homePositionChanged(false)
{
}

QnPtzManageModel::~QnPtzManageModel() {
}

const QnPtzManageModelColors QnPtzManageModel::colors() const {
    return m_colors;
}

void QnPtzManageModel::setColors(const QnPtzManageModelColors &colors) {
    m_colors = colors;

    // TODO: #Elric emit data change
}

const QList<QnPtzTourItemModel> &QnPtzManageModel::tourModels() const {
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
    cleanHotkeys();
    endResetModel();
}

void QnPtzManageModel::addTour() {
    int firstRow = rowCount();
    int lastRow = firstRow;
    if (m_tours.isEmpty())
        lastRow++;

    QString name = generateUniqueString(collectTourNames(), QString(), tr("New tour %1"));

    beginInsertRows(QModelIndex(), firstRow, lastRow);
    m_tours << name;
    m_tours.last().modified = true;
    endInsertRows();
}

void QnPtzManageModel::removeTour(const QString &id) {
    int idx = tourIndex(id);
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

QnPtzPresetList QnPtzManageModel::presets() const {
    return m_ptzPresetsCache;
}

const QList<QnPtzPresetItemModel> & QnPtzManageModel::presetModels() const {
    return m_presets;
}

void QnPtzManageModel::setPresets(const QnPtzPresetList &presets) {
    if (m_ptzPresetsCache == presets)
        return;

    beginResetModel();
    m_presets.clear();
    m_ptzPresetsCache.clear();
    foreach (const QnPtzPreset &preset, presets)
        m_presets << preset;
    cleanHotkeys();
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

    QString name = generateUniqueString(collectPresetNames(), QString(), tr("Saved position %1"));

    beginInsertRows(QModelIndex(), firstRow, lastRow);
    m_presets << name;
    m_presets.last().modified = true;
    endInsertRows();

    updatePresetsCache();
}

void QnPtzManageModel::removePreset(const QString &id) {
    int idx = presetIndex(id);
    if (idx < 0)
        return;

    int firstRow = idx;
    if (m_presets.size() > 1)
        firstRow++; // do not remove presets title
    int lastRow = idx + 1; // +1 for presets title

    beginRemoveRows(QModelIndex(), firstRow, lastRow);
    m_removedPresets << m_presets.takeAt(idx).preset.id;
    cleanHotkeys();
    endRemoveRows();
   
    updatePresetsCache();
}

void QnPtzManageModel::setHotkeys(const QnPtzHotkeyHash &hotkeys) {
    if (m_hotkeys == hotkeys)
        return;
    m_hotkeys = hotkeys;
    cleanHotkeys();
    emit dataChanged(index(0, HotkeyColumn), index(rowCount(), HotkeyColumn));
}

const QnPtzHotkeyHash& QnPtzManageModel::hotkeys() const {
    return m_hotkeys;
}

const QString QnPtzManageModel::homePosition() const {
    return m_homePosition;
}

void QnPtzManageModel::setHomePosition(const QString &homePosition) {
    setHomePositionInternal(homePosition, false);
}

bool QnPtzManageModel::isHomePositionChanged() const {
    return m_homePositionChanged;
}

void QnPtzManageModel::setHomePositionInternal(const QString &homePosition, bool setChanged) {
    m_homePositionChanged = setChanged;

    if (m_homePosition == homePosition)
        return;

    int oldPos = rowNumber(rowData(m_homePosition));

    m_homePosition = homePosition;

    int newPos = rowNumber(rowData(m_homePosition));

    if (oldPos >= 0)
        emit dataChanged(index(oldPos, HomeColumn), index(oldPos, HomeColumn));
    if (newPos >= 0)
        emit dataChanged(index(newPos, HomeColumn), index(newPos, HomeColumn));
}

bool QnPtzManageModel::setHotkeyInternal(int hotkey, const QString &id) { 
    if (id.isEmpty() || m_hotkeys.value(hotkey) == id)
        return false;

    /* Clean previous hotkeys. */
    for (int key: m_hotkeys.keys(id))
        m_hotkeys.remove(key);

    /* Set new hotkey. */
    if (hotkey != QnPtzHotkey::NoHotkey) 
        m_hotkeys.insert(hotkey, id);

    { /* Mark preset as modified (if any). */
        int idx = presetIndex(id);
        if (idx >= 0)
            m_presets[idx].modified = true;
    }
    { /* Mark tour as modified (if any). */
        int idx = tourIndex(id);
        if (idx >= 0)
            m_tours[idx].modified = true;
    }
    return true;
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
    cleanHotkeys();
    endRemoveRows();

    return true;
}

QVariant QnPtzManageModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.model() != this || !hasIndex(index.row(), index.column(), index.parent())) {
        if (role == Qn::HelpTopicIdRole)
            return Qn::PtzPresets_Help;
        return QVariant();
    }
    
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
        bool checked = (value.toInt() == Qt::Checked);
        setHomePositionInternal(checked ? data.id() : QString(), true);
        return true;
    } else if (role == Qt::EditRole && index.column() == HotkeyColumn) {
        bool ok = false;
        int hotkey = value.toInt(&ok);
        if (!ok || ((hotkey > 9 || hotkey < 0) && hotkey != QnPtzHotkey::NoHotkey))
            return false;

        /* Check if hotkey is already in use. */
        if (hotkey != QnPtzHotkey::NoHotkey) {
            QString currentId = m_hotkeys.value(hotkey);
            if (!currentId.isEmpty() && currentId != data.id())
                return false;
        }

        if (setHotkeyInternal(hotkey, data.id())) {
            emit dataChanged(index, index);
            emit dataChanged(index.sibling(index.row(), ModifiedColumn), index.sibling(index.row(), ModifiedColumn));
        }
        return true;
    } else if (role == Qt::EditRole && index.column() == NameColumn) {
        QString newName = value.toString().trimmed();
        if (newName.isEmpty())
            return false;

        switch (data.rowType) {
        case QnPtzManageModel::PresetRow: 
            {
                int idx = presetIndex(data.presetModel.preset.id);
                if (idx < 0)
                    return false;
                QnPtzPresetItemModel &model = m_presets[idx];
                if (model.preset.name == newName)
                    return false;
                model.preset.name = value.toString();
                model.modified = true;

                emit dataChanged(index, index);
                return true;
            }
        case QnPtzManageModel::TourRow:
            {
                int idx = tourIndex(data.tourModel.tour.id);
                if (idx < 0)
                    return false;
                QnPtzTourItemModel &model = m_tours[idx];
                if (model.tour.name == newName)
                    return false;
                model.tour.name = value.toString();
                model.modified = true;

                emit dataChanged(index, index);
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

QnPtzManageModel::RowData QnPtzManageModel::rowData(const QString &id, int *index) const {
    RowData rowData;

    int idx = presetIndex(id);
    if (idx >= 0) {
        rowData.rowType = PresetRow;
        rowData.presetModel = m_presets[idx];
    } else {
        idx = tourIndex(id);
        if (idx >= 0) {
            rowData.rowType = TourRow;
            rowData.tourModel = m_tours[idx];
        }
    }
    if (idx >= 0 && index)
        *index = idx;

    return rowData;
}

int QnPtzManageModel::rowNumber(const QnPtzManageModel::RowData &rowData) const {
    if (rowData.rowType == PresetTitleRow)
        return 0;

    int offset = 1;
    if (rowData.rowType == PresetRow) {
        int idx = presetIndex(rowData.presetModel.preset.id);
        if (idx < 0)
            return -1;
        return offset + idx;
    }

    offset += m_presets.size();

    if (rowData.rowType == TourTitleRow)
        return offset;

    ++offset;
    if (rowData.rowType == TourRow) {
        int idx = tourIndex(rowData.tourModel.tour.id);
        if (idx < 0)
            return -1;
        return offset + idx;
    }

    return -1;
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
        return m_colors.title;
    case Qt::FontRole: 
        {
            QFont f;
            f.setBold(true);
            return f;
        }
    case Qn::HelpTopicIdRole:
        return rowType == PresetTitleRow ? Qn::PtzPresets_Help : Qn::PtzManagement_Tour_Help;
        
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
        case HotkeyColumn: {
            int hotkey = m_hotkeys.key(presetModel.preset.id, QnPtzHotkey::NoHotkey);
            return hotkey < 0 ? tr("None") : QString::number(hotkey);
        }
        case HomeColumn:
            if (role == Qt::ToolTipRole)
                return tr("This preset will be activated after %n minutes of inactivity", 0, 5); // TODO: #Elric #PTZ use value from home PTZ controller
            break;
        case DetailsColumn:
            return QVariant();
        default:
            break;
        }
        return QVariant();
    case Qt::CheckStateRole:
        if (column == HomeColumn)
            return m_homePosition == presetModel.preset.id ? Qt::Checked : Qt::Unchecked;
        break;
    case Qn::PtzPresetRole:
        return QVariant::fromValue<QnPtzPreset>(presetModel.preset);
    case Qn::ValidRole:
        return true;
    case Qn::HelpTopicIdRole:
        return column == HomeColumn ? Qn::PtzManagement_HomePosition_Help : Qn::PtzPresets_Help;
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
        case DetailsColumn: {
            QString tourStateString;
            tourState(tourModel, &tourStateString);
            return tourStateString;
        }
        default:
            break;
        }
        return QVariant();

    case Qt::BackgroundRole:
        switch (tourState(tourModel)) {
        case IncompleteTour:
        case OtherInvalidTour:
            return m_colors.invalid;
        case DuplicatedLinesTour:
            return m_colors.warning;
        default:
            break;
        }
        break;
    case Qt::CheckStateRole:
        if (column == HomeColumn)
            return m_homePosition == tourModel.tour.id ? Qt::Checked : Qt::Unchecked;
        break;
    case Qn::PtzTourRole:
        return QVariant::fromValue<QnPtzTour>(tourModel.tour);
    case Qn::ValidRole:
        //TODO: some gradations required: fully invalid, only warning (eg. hotkey duplicates)
        return tourIsValid(tourModel);
    case Qn::HelpTopicIdRole:
        return column == HomeColumn ? Qn::PtzManagement_HomePosition_Help : Qn::PtzManagement_Tour_Help;
    default:
        break;
    }
    return QVariant();
}

int QnPtzManageModel::presetIndex(const QString &id) const {
    return qnIndexOf(m_presets, [&](const QnPtzPresetItemModel &model) {return id == model.preset.id; });
}

int QnPtzManageModel::tourIndex(const QString &id) const {
    return qnIndexOf(m_tours, [&](const QnPtzTourItemModel &model) {return id == model.tour.id; });
}

bool QnPtzManageModel::tourIsValid(const QnPtzTourItemModel &tourModel) const {
    return tourModel.tour.isValid(m_ptzPresetsCache);
}

QnPtzManageModel::TourState QnPtzManageModel::tourState(const QnPtzTourItemModel &tourModel, QString *stateString) const {
    if (tourModel.tour.spots.size() < 2) {
        if (stateString)
            *stateString = tr("Tour should contain at least 2 positions");
        return IncompleteTour;
    }

    if (!tourIsValid(tourModel)) {
        if (stateString)
            *stateString = tr("Invalid tour");
        return OtherInvalidTour;
    }

    const QnPtzTourSpotList &spots = tourModel.tour.spots;
    for (int i = 0; i < spots.size(); ++i) {
        int j;
        for(j = (i + 1) % spots.size(); j != i; j = (j + 1) % spots.size())
            if(spots[i].presetId != spots[j].presetId)
                break;

        int count = i < j ? j - i : j - i + spots.size();
        if(count >= 2) {
            if (stateString)
                *stateString = tr("Tour has %n identical positions", 0, count);
            return DuplicatedLinesTour;
        }
    }

    if (stateString) {
        qint64 time = estimatedTimeSecs(tourModel.tour);
        *stateString = tr("Tour time: %1").arg((time < 60) ? tr("less than a minute") : tr("about %n minute(s)", 0, time / 60));
    }
    return ValidTour;
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

    return m_removedPresets.isEmpty() && m_removedTours.isEmpty() && !m_homePositionChanged;
}

void QnPtzManageModel::setSynchronized() {
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

    // synchronized() should be true after the model has been reset
    m_removedPresets.clear();
    m_removedTours.clear();
    m_homePositionChanged = false;

    endResetModel();
}

QStringList QnPtzManageModel::collectTourNames() const {
    QStringList result;
    foreach(const QnPtzTourItemModel &tour, m_tours)
        result.push_back(tour.tour.name);
    return result;
}

QStringList QnPtzManageModel::collectPresetNames() const {
    QStringList result;
    foreach(const QnPtzPresetItemModel &preset, m_presets)
        result.push_back(preset.preset.name);
    return result;
}

void QnPtzManageModel::cleanHotkeys() {
    for(auto iter = m_hotkeys.begin(); iter != m_hotkeys.end();) {
        QString id = iter.value();
        if (presetIndex(id) >= 0 || tourIndex(id) >= 0)
            ++iter;
        else
            iter = m_hotkeys.erase(iter);
    } 
}

QString QnPtzManageModel::RowData::id() const {
    switch (rowType) {
    case PresetRow:
        return presetModel.preset.id;
    case TourRow:
        return tourModel.tour.id;
    default:
        return QString();
    }
}

QString QnPtzManageModel::RowData::name() const {
    switch (rowType) {
    case PresetRow:
        return presetModel.preset.name;
    case TourRow:
        return tourModel.tour.name;
    default:
        return QString();
    }
}

bool QnPtzManageModel::RowData::modified() const {
    switch (rowType) {
    case PresetRow:
        return presetModel.modified;
    case TourRow:
        return tourModel.modified;
    default:
        return false;
    }
}

bool QnPtzManageModel::RowData::local() const {
    switch (rowType) {
    case PresetRow:
        return presetModel.local;
    case TourRow:
        return tourModel.local;
    default:
        return false;
    }
}
