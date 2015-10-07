#include "storage_list_model.h"
#include <ui/workbench/workbench_access_controller.h>

// ------------------ QnStorageListMode --------------------------

QnStorageListModel::QnStorageListModel(QObject *parent)
    : base_type(parent)
    , QnWorkbenchContextAware(parent)
{
}

QnStorageListModel::~QnStorageListModel() {}

void QnStorageListModel::setModelData(const QnStorageSpaceReply& data)
{
    m_data = data;
}

int QnStorageListModel::rowCount(const QModelIndex &parent) const {
    if (!parent.isValid())
        return m_data.storages.size();
    return 0;
}

int QnStorageListModel::columnCount(const QModelIndex &parent) const {
    if (!parent.isValid())
        return ColumnCount;
    return 0;
}

QVariant QnStorageListModel::data(const QModelIndex &index, int role) const {
    if (!hasIndex(index.row(), index.column(), index.parent()))
        return QVariant();

    const QnStorageSpaceData& storageData = m_data.storages[index.row()];

    return QVariant();
}

QVariant QnStorageListModel::headerData(int section, Qt::Orientation orientation, int role) const 
{
    // column headers aren't used so far
    return base_type::headerData(section, orientation, role);
}

Qt::ItemFlags QnStorageListModel::flags(const QModelIndex &index) const {
    Qt::ItemFlags flags = Qt::NoItemFlags;
    flags |= Qt::ItemIsSelectable | Qt::ItemIsEnabled;

    if (index.column() == CheckBoxColumn)
        flags |= Qt::ItemIsUserCheckable;

    return flags;
}
