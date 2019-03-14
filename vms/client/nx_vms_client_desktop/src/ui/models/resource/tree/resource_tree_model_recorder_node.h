#pragma once

#include <ui/models/resource/resource_tree_model_node.h>

class QnResourceTreeModelRecorderNode: public QnResourceTreeModelNode
{
    using base_type = QnResourceTreeModelNode;
public:
    QnResourceTreeModelRecorderNode(QnResourceTreeModel* model,
        const QnVirtualCameraResourcePtr& camera);
    virtual ~QnResourceTreeModelRecorderNode();

    virtual void addChildInternal(const QnResourceTreeModelNodePtr& child);
    virtual void removeChildInternal(const QnResourceTreeModelNodePtr& child);

protected:
    virtual QIcon calculateIcon() const override;
    virtual nx::vms::client::desktop::CameraExtraStatus calculateCameraExtraStatus() const override;

private:
    void updateName(const QnVirtualCameraResourcePtr& camera);
};
