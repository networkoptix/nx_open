#include "plain_resource_model_node.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/camera_resource.h>
#include <mobile_client/mobile_client_roles.h>
#include <models/plain_resource_model.h>

QnPlainResourceModelNode::QnPlainResourceModelNode(const QnResourcePtr &resource) :
    m_resource(resource)
{
    if (m_resource->hasFlags(Qn::server))
        m_type = QnPlainResourceModel::ServerNode;
    else if (m_resource.dynamicCast<QnVirtualCameraResource>())
        m_type = QnPlainResourceModel::CameraNode;
    m_id = m_resource->getId();
    update();
}

QnUuid QnPlainResourceModelNode::id() const {
    return m_id;
}

QString QnPlainResourceModelNode::name() const {
    return m_name;
}

int QnPlainResourceModelNode::type() const {
    return m_type;
}

QnMediaServerResourcePtr QnPlainResourceModelNode::server() const {
    if (type() == QnPlainResourceModel::ServerNode)
        return m_resource.dynamicCast<QnMediaServerResource>();
    else
        return qnResPool->getResourceById(m_resource->getParentId()).dynamicCast<QnMediaServerResource>();
}

QVariant QnPlainResourceModelNode::data(int role) const {
    switch (role) {
    case Qt::DisplayRole:
        return name();
    case Qn::NodeTypeRole:
        return type();
    case Qn::UuidRole:
        return id().getQUuid();
    case Qn::ServerResourceRole:
        return QVariant::fromValue(server());
    case Qn::ServerNameRole:
        return server()->getName();
    default:
        return QVariant();
    }
}

void QnPlainResourceModelNode::update() {
    if (!m_resource)
        return;

    m_name = m_resource->getName();
}
