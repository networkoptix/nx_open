#include "ptz_preset_list_model.h"

#include <common/common_globals.h>

#if 0
QnPtzPresetListModel::QnPtzPresetListModel(QObject *parent):
    base_type(parent),
    m_readOnly(false),
    m_duplicateHotkeysEnabled(false)
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

bool QnPtzPresetListModel::isDuplicateHotkeysEnabled() {
    return m_duplicateHotkeysEnabled;
}

void QnPtzPresetListModel::setDuplicateHotkeysEnabled(bool duplicateHotkeysEnabled) {
    m_duplicateHotkeysEnabled = duplicateHotkeysEnabled;
}

const QList<QnPtzPreset> &QnPtzPresetListModel::presets() const {
    return m_presets;
}

void QnPtzPresetListModel::setPresets(const QList<QnPtzPreset> &presets) {
    beginResetModel();
    m_presets = presets;
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
            return preset.hotkey < 0 ? tr("None") : QString::number(preset.hotkey);
        default:
            break;
        }
        break;
    case Qt::EditRole:
        switch(column) {
        case NameColumn:
            return preset.name;
        case HotkeyColumn:
            return preset.hotkey;
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

        if(hotkey >= 0 && !m_duplicateHotkeysEnabled) {
            for(int i = 0; i < m_presets.size(); i++) {
                if(m_presets[i].hotkey == hotkey) {
                    m_presets[i].hotkey = preset.hotkey;
                    QModelIndex siblingIndex = index.sibling(i, index.column());
                    emit dataChanged(siblingIndex, siblingIndex);
                }
            }
        }

        preset.hotkey = hotkey;
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
#endif