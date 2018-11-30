#include "resource_tree_model_recorder_node.h"

#include <core/resource/camera_resource.h>

#include <ui/style/resource_icon_cache.h>

using namespace nx::vms::client::desktop;

QnResourceTreeModelRecorderNode::QnResourceTreeModelRecorderNode(
    QnResourceTreeModel* model,
    const QnVirtualCameraResourcePtr& camera)
    :
    base_type(model, NodeType::recorder, QString())
{
    updateName(camera);
}

QnResourceTreeModelRecorderNode::~QnResourceTreeModelRecorderNode()
{
}

void QnResourceTreeModelRecorderNode::addChildInternal(const QnResourceTreeModelNodePtr& child)
{
    const bool addingFirstChild = children().empty();
    base_type::addChildInternal(child);

    if (addingFirstChild)
    {
        auto camera = child->resource().dynamicCast<QnVirtualCameraResource>();
        NX_ASSERT(camera);
        if (!camera)
            return;

        connect(camera, &QnVirtualCameraResource::groupNameChanged, this,
            [this, camera]()
            {
                updateName(camera);
            });

        connect(camera, &QnVirtualCameraResource::propertyChanged, this,
            [this, camera](const QnResourcePtr& /*resource*/, const QString& name)
            {
                const bool hasDeviceTypeBeenChanged = name == ResourcePropertyKey::kDts
                    || name == ResourcePropertyKey::kAnalog
                    || name == ResourcePropertyKey::kDeviceType;

                if (hasDeviceTypeBeenChanged)
                    updateIcon();
            });

        updateIcon();
    }
    updateCameraExtraStatus();
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

QIcon QnResourceTreeModelRecorderNode::calculateIcon() const
{
    if (!children().empty())
    {
        const auto camera = child(0)->resource().dynamicCast<QnVirtualCameraResource>();
        NX_ASSERT(camera);
        if (camera && camera->isMultiSensorCamera())
            return qnResIconCache->icon(QnResourceIconCache::MultisensorCamera);
    }

    return qnResIconCache->icon(QnResourceIconCache::Recorder);
}

CameraExtraStatus QnResourceTreeModelRecorderNode::calculateCameraExtraStatus() const
{
    CameraExtraStatus result;
    for (auto child: children())
        result |= child->data(Qn::CameraExtraStatusRole, 0).value<CameraExtraStatus>();
    return result;
}

void QnResourceTreeModelRecorderNode::updateName(const QnVirtualCameraResourcePtr& camera)
{
    auto name = camera->getUserDefinedGroupName();
    if (name.isEmpty())
        name = camera->getGroupId();
    NX_ASSERT(!name.isEmpty());
    setName(name);
    changeInternal();
}
