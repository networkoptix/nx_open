#include "backup_settings_model.h"
#include <ui/workbench/workbench_access_controller.h>
#include "ui/style/resource_icon_cache.h"
#include "ui/common/ui_resource_name.h"
#include "core/resource_management/resource_pool.h"

// ------------------ QnStorageListMode --------------------------

QnBackupSettingsModel::QnBackupSettingsModel(QObject *parent)
    : base_type(parent)
{
}

QnBackupSettingsModel::~QnBackupSettingsModel() {}

void QnBackupSettingsModel::setModelData(const BackupSettingsDataList& data)
{
    beginResetModel();
    m_data = data;
    endResetModel();
}

BackupSettingsDataList QnBackupSettingsModel::modelData() const
{
    return m_data;
}

int QnBackupSettingsModel::rowCount(const QModelIndex &parent) const {
    if (!parent.isValid())
        return m_data.size();
    return 0;
}

int QnBackupSettingsModel::columnCount(const QModelIndex &parent) const {
    if (!parent.isValid())
        return ColumnCount;
    return 0;
}

QString QnBackupSettingsModel::backupTypeToString(Qn::CameraBackupTypes value) const
{
    if (value == Qn::CameraBackup_Both)
        return tr("All");
    else if (value == Qn::CameraBackup_HighQuality)
        return tr("High");
    else if (value == Qn::CameraBackup_LowQuality)
        return tr("Low");
    else
        return tr("---");
}

QString QnBackupSettingsModel::displayData(const QModelIndex &index) const
{
    const BackupSettingsData& data = m_data[index.row()];
    switch (index.column())
    {
    case CheckBoxColumn:
        return QString();
    case CameraNameColumn:
        return getResourceName(qnResPool->getResourceById(data.id));
    case BackupTypeColumn:
        return backupTypeToString(data.backupType);
    default:
        break;
    }

    return QString();
}

QVariant QnBackupSettingsModel::fontData(const QModelIndex &index) const 
{
    return QVariant();
}

QVariant QnBackupSettingsModel::foregroundData(const QModelIndex &index) const 
{
    return QVariant();
}

QVariant QnBackupSettingsModel::checkstateData(const QModelIndex &index) const
{
    const BackupSettingsData& data = m_data[index.row()];
    if (index.column() == CheckBoxColumn)
        return data.isChecked ? Qt::Checked : Qt::Unchecked;
    return QVariant();
}

QVariant QnBackupSettingsModel::data(const QModelIndex &index, int role) const 
{
    if (!hasIndex(index.row(), index.column(), index.parent()))
        return QVariant();

    switch(role) {
    case Qt::DecorationRole:
        if (index.column() == CameraNameColumn)
            return qnResIconCache->icon(qnResPool->getResourceById(m_data[index.row()].id));
        else
            return QVariant();
    case Qt::DisplayRole:
    case Qn::DisplayHtmlRole:
        return displayData(index);
    case Qt::FontRole:
        return fontData(index);
    case Qt::ForegroundRole:
        return foregroundData(index);
    case Qt::CheckStateRole:
        return checkstateData(index);
    default:
        break;
    }
    return QVariant();

    return QVariant();
}

void QnBackupSettingsModel::setData(const QModelIndexList &indexList, const QVariant &value, int role)
{
    beginResetModel();
    blockSignals(true);
    for (const auto& index: indexList)
        setData(index, value, role);
    blockSignals(false);
    endResetModel();
}

bool QnBackupSettingsModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || index.model() != this || !hasIndex(index.row(), index.column(), index.parent()))
        return false;

    BackupSettingsData& data = m_data[index.row()];

    if (role == Qt::CheckStateRole)
    {
        data.isChecked = (value == Qt::Checked);
        emit dataChanged(index, index, QVector<int>() << role);
        return true;
    }
    else {
        return base_type::setData(index, value, role);
    }
}

Qt::ItemFlags QnBackupSettingsModel::flags(const QModelIndex &index) const {
    Qt::ItemFlags flags = Qt::NoItemFlags;
    flags |= Qt::ItemIsSelectable | Qt::ItemIsEnabled;

    if (index.column() == CheckBoxColumn)
        flags |= Qt::ItemIsUserCheckable;

    return flags;
}

QVariant QnBackupSettingsModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (section >= ColumnCount)
        return QVariant();

    switch((Columns) section) 
    {
        case CheckBoxColumn:
            return QVariant();
        case CameraNameColumn:
            return tr("Camera name");
        case BackupTypeColumn:
            return tr("Quality");
        default:
            return QVariant();
    }
    return QVariant();
}

void QnBackupSettingsModel::setCheckState(Qt::CheckState state)
{
    if (state == Qt::PartiallyChecked)
        return;

    beginResetModel();
    for (auto& value: m_data)
        value.isChecked = state == Qt::Checked ? true : false;
    endResetModel();
    //emit dataChanged(index(0,0), index(m_data.size(), ColumnCount), QVector<int>() << Qt::CheckStateRole);
}

Qt::CheckState QnBackupSettingsModel::checkState() const
{
    bool hasChecked = false;
    bool hasUnchecked = false;
    for (const auto& value: m_data) {
        if (value.isChecked)
            hasChecked = true;
        else
            hasUnchecked = true;
    }

    if (hasChecked && hasUnchecked)
        return Qt::PartiallyChecked;
    else if (hasChecked)
        return Qt::Checked;
    else
        return Qt::Unchecked;
}
