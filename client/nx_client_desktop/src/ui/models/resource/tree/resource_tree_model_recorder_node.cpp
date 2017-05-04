#include "resource_tree_model_recorder_node.h"

#include <core/resource/camera_resource.h>

QnResourceTreeModelRecorderNode::QnResourceTreeModelRecorderNode(
    QnResourceTreeModel* model,
    const QnVirtualCameraResourcePtr& camera)
    :
    base_type(model, Qn::RecorderNode, QString())
{
    updateName(camera);
}

QnResourceTreeModelRecorderNode::~QnResourceTreeModelRecorderNode()
{
}

void QnResourceTreeModelRecorderNode::addChildInternal(const QnResourceTreeModelNodePtr& child)
{
    base_type::addChildInternal(child);
    auto camera = child->resource().dynamicCast<QnVirtualCameraResource>();
    NX_ASSERT(camera);
    connect(camera, &QnVirtualCameraResource::groupNameChanged, this,
        [this, camera]()
        {
            updateName(camera);
        });
}

void QnResourceTreeModelRecorderNode::removeChildInternal(const QnResourceTreeModelNodePtr& child)
{
    base_type::removeChildInternal(child);
    NX_ASSERT(child->resource());
    if (child->resource())
    {
        auto camera = child->resource().dynamicCast<QnVirtualCameraResource>();
        NX_ASSERT(camera);
        disconnect(camera, nullptr, this, nullptr);
    }
}

void QnResourceTreeModelRecorderNode::updateName(const QnVirtualCameraResourcePtr& camera)
{
    auto name = camera->getGroupName();
    if (name.isEmpty())
        name = camera->getGroupId();
    NX_ASSERT(!name.isEmpty());
    setName(name);
    changeInternal();
}
