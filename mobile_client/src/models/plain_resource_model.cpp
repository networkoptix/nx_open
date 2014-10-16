#include "plain_resource_model.h"

#include "core/resource_management/resource_pool.h"
#include "core/resource/media_server_resource.h"
#include "core/resource/camera_resource.h"
#include "mobile_client/mobile_client_roles.h"
#include "plain_resource_model_node.h"

QnPlainResourceModel::QnPlainResourceModel(QObject *parent) :
    QAbstractListModel(parent)
{
    connect(qnResPool,      &QnResourcePool::resourceAdded,     this,   &QnPlainResourceModel::at_resourcePool_resourceAdded);
    connect(qnResPool,      &QnResourcePool::resourceRemoved,   this,   &QnPlainResourceModel::at_resourcePool_resourceRemoved);
    connect(qnResPool,      &QnResourcePool::resourceChanged,   this,   &QnPlainResourceModel::at_resourcePool_resourceChanged);

    foreach (const QnResourcePtr &resource, qnResPool->getResources())
        at_resourcePool_resourceAdded(resource);
}

int QnPlainResourceModel::rowCount(const QModelIndex &parent) const {
    Q_UNUSED(parent)
    return m_itemOrder.size();
}

QVariant QnPlainResourceModel::data(const QModelIndex &index, int role) const {
    if (!hasIndex(index.row(), index.column(), index.parent()))
        return QVariant();

    QnPlainResourceModelNode *node = this->node(index);
    if (!node)
        return QVariant();

    return node->data(role);
}

QHash<int, QByteArray> QnPlainResourceModel::roleNames() const {
    QHash<int, QByteArray> roleNames = QAbstractListModel::roleNames();

    roleNames[Qn::NodeTypeRole] = Qn::roleName(Qn::NodeTypeRole);
    roleNames[Qn::ThumbnailRole] = Qn::roleName(Qn::ThumbnailRole);
    roleNames[Qn::UuidRole] = Qn::roleName(Qn::UuidRole);

    return roleNames;
}

QModelIndex QnPlainResourceModel::index(int row, int column, const QModelIndex &parent) const {
    if (!hasIndex(row, column, parent))
        return QModelIndex();
    QnUuid id = m_itemOrder[row];
    return index(m_nodeById.value(id));
}

void QnPlainResourceModel::at_resourcePool_resourceAdded(const QnResourcePtr &resource) {
    /* Only servers and cameras are supported */
    if (!resource->hasFlags(Qn::server) && !resource.dynamicCast<QnVirtualCameraResource>())
        return;

    QnPlainResourceModelNode *node = this->node(resource);
    QModelIndex index = this->index(node);
    if (!index.isValid()) {
        int row = m_itemOrder.size();
        beginInsertRows(QModelIndex(), row, row);
        m_itemOrder.append(node->id());
        endInsertRows();
    }
}

void QnPlainResourceModel::at_resourcePool_resourceRemoved(const QnResourcePtr &resource) {
    QnPlainResourceModelNode *node = this->existentNode(resource);
    if (!node)
        return;

    QModelIndex index = this->index(node);
    beginRemoveRows(QModelIndex(), index.row(), index.row());
    m_itemOrder.removeAt(index.row());
    endRemoveRows();

    m_nodeById.remove(node->id());
    delete node;
}

void QnPlainResourceModel::at_resourcePool_resourceChanged(const QnResourcePtr &resource) {
    QnPlainResourceModelNode *node = this->existentNode(resource);
    if (!node)
        return;

    node->update();
    QModelIndex index = this->index(node);
    emit dataChanged(index, index);
}

QModelIndex QnPlainResourceModel::index(QnPlainResourceModelNode *node) const {
    int row = m_itemOrder.indexOf(node->id());
    if (row == -1)
        return QModelIndex();
    return createIndex(row, 0, node);
}

QnPlainResourceModelNode *QnPlainResourceModel::existentNode(const QnResourcePtr &resource) {
    return m_nodeById.value(resource->getId());
}

QnPlainResourceModelNode *QnPlainResourceModel::node(const QnResourcePtr &resource) {
    QnPlainResourceModelNode *node = existentNode(resource);
    if (node)
        return node;

    node = new QnPlainResourceModelNode(resource);
    m_nodeById[node->id()] = node;
    return node;
}

QnPlainResourceModelNode *QnPlainResourceModel::node(const QModelIndex &index) const {
    return reinterpret_cast<QnPlainResourceModelNode*>(index.internalPointer());
}
