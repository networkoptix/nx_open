#include "ptz_preset_list_model.h"

#include <common/common_globals.h>
#include <core/ptz/ptz_preset.h>
#include <utils/common/string.h>

QnPtzPresetListModel::QnPtzPresetListModel(QObject *parent):
    base_type(parent),
    m_readOnly(false)
{
    QList<Column> columns;
    columns << NameColumn << HotkeyColumn;
    setColumns(columns);
}

QnPtzPresetListModel::~QnPtzPresetListModel() {
    return;
}

bool QnPtzPresetListModel::isReadOnly() const {
    return m_readOnly;
}

void QnPtzPresetListModel::setReadOnly(bool readOnly) {
    m_readOnly = readOnly;
}

const QnPtzPresetList &QnPtzPresetListModel::presets() const {
    return m_presets;
}

void QnPtzPresetListModel::setPresets(const QnPtzPresetList &presets) {
    beginResetModel();
    m_presets = presets;
    qSort(m_presets.begin(), m_presets.end(), [](const QnPtzPreset &l, const QnPtzPreset &r) {
        return naturalStringCaseInsensitiveLessThan(l.name, r.name);
    });
    endResetModel();
}

QnPtzHotkeyHash QnPtzPresetListModel::hotkeys() const {
    return m_hotkeys;
}

void QnPtzPresetListModel::setHotkeys(const QnPtzHotkeyHash &value) {
    beginResetModel();
    m_hotkeys = value;
    endResetModel();
}

int QnPtzPresetListModel::column(Column column) const {
    return m_columns.indexOf(column);
}

const QList<QnPtzPresetListModel::Column> &QnPtzPresetListModel::columns() const {
    return m_columns;
}

void QnPtzPresetListModel::setColumns(const QList<Column> &columns) {
    if(m_columns == columns)
        return;

    beginResetModel();
    m_columns = columns;
    endResetModel();
}

int QnPtzPresetListModel::rowCount(const QModelIndex &parent) const {
    if(!parent.isValid())
        return m_presets.size();

    return 0;
}

int QnPtzPresetListModel::columnCount(const QModelIndex &parent) const {
    if(!parent.isValid())
        return m_columns.size();

    return 0;
}

Qt::ItemFlags QnPtzPresetListModel::flags(const QModelIndex &index) const {
    Qt::ItemFlags result = base_type::flags(index);
    if(m_readOnly)
        return result;

    if(!index.isValid())
        return result;

    if(!hasIndex(index.row(), index.column(), index.parent()))
        return result;

    return result | Qt::ItemIsEditable;
}

QVariant QnPtzPresetListModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.model() != this || !hasIndex(index.row(), index.column(), index.parent()))
        return QVariant();

    const QnPtzPreset &preset = m_presets[index.row()];
    int hotkey = m_hotkeys.key(preset.id, QnPtzHotkey::NoHotkey);

    Column column = m_columns[index.column()];

    switch(role) {
    case Qt::DisplayRole:
    case Qt::ToolTipRole:
    case Qt::StatusTipRole:
    case Qt::WhatsThisRole:
    case Qt::AccessibleTextRole:
    case Qt::AccessibleDescriptionRole:
        switch(column) {
        case NameColumn: 
            return preset.name;
        case HotkeyColumn:
            return hotkey < 0 ? tr("None") : QString::number(hotkey);
        default:
            break;
        }
        break;
    case Qt::EditRole:
        switch(column) {
        case NameColumn:
            return preset.name;
        case HotkeyColumn:
            return hotkey;
        default:
            break;
        }
        break;
    case Qn::PtzPresetRole:
        return QVariant::fromValue<QnPtzPreset>(preset);
    default:
        break;
    }

    return QVariant();
}

bool QnPtzPresetListModel::setData(const QModelIndex &index, const QVariant &value, int role) {
    if(role != Qt::EditRole)
        return false;

    if(!index.isValid())
        return false;

    if(!hasIndex(index.row(), index.column(), index.parent()))
        return false;

    QnPtzPreset &preset = m_presets[index.row()];
    Column column = m_columns[index.column()];

    switch(column) {
    case NameColumn: {
        QString name = value.toString();
        if(name.isEmpty())
            return false;

        if(preset.name == name)
            return true;

        preset.name = name;
        emit dataChanged(index, index);
        return true;
    }
    case HotkeyColumn: {
        bool ok = false;
        int hotkey = value.toInt(&ok);
        if(!ok || hotkey > 9)
            return false;

        // hotkey that was assigned to this preset
        int oldHotkey = m_hotkeys.key(preset.id, QnPtzHotkey::NoHotkey);
        if (oldHotkey == hotkey)
            return false;

        // preset that is assigned to this hotkey
        QString existing;
        if (hotkey != QnPtzHotkey::NoHotkey)
            existing = m_hotkeys[hotkey];

        // set old hotkey to an existing preset (or empty)
        if (oldHotkey != QnPtzHotkey::NoHotkey)
            m_hotkeys[oldHotkey] = existing;
        if (!existing.isEmpty()) {
            for(int i = 0; i < m_presets.size(); i++) {
                if (m_presets[i].id != existing)
                    continue;
                QModelIndex siblingIndex = index.sibling(i, index.column());
                emit dataChanged(siblingIndex, siblingIndex);
                break;
            }
        }

        // set updated hotkey
        if (hotkey != QnPtzHotkey::NoHotkey)
            m_hotkeys[hotkey] = preset.id;
        emit dataChanged(index, index);

        return true;
    }
    default:
        return base_type::setData(index, value, role);
    }
}

QVariant QnPtzPresetListModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole && section >= 0 && section < m_columns.size())
        return columnTitle(m_columns[section]);
    return base_type::headerData(section, orientation, role);
}

bool QnPtzPresetListModel::removeRows(int row, int count, const QModelIndex &parent) {
    if(parent.isValid() || row < 0 || count < 1 || row + count > m_presets.size())
        return false;

    beginRemoveRows(parent, row, row + count - 1);
    m_presets.erase(m_presets.begin() + row, m_presets.begin() + row + count);
    endRemoveRows();
    return true;
}

bool QnPtzPresetListModel::insertRows(int row, int count, const QModelIndex &parent) {
    if(parent.isValid() || row < 0 || row > m_presets.size() || count < 1)
        return false;

    beginInsertRows(parent, row, row + count - 1);
    for(int i = 0; i < count; i++)
        m_presets.insert(row, QnPtzPreset());
    endInsertRows();
    return true;
}

QString QnPtzPresetListModel::columnTitle(Column column) const {
    switch(column) {
    case NameColumn: return tr("Name");
    case HotkeyColumn: return tr("Hotkey");
    default: return QString();
    }
}
