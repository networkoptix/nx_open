#pragma once

#include <core/resource/resource_fwd.h>
#include <nx/client/desktop/resource_views/node_view/nodes/view_node.h>

namespace nx {
namespace client {
namespace desktop {

class ResourceNode: public ViewNode
{
    using base_type = ViewNode;

public:
    static NodePtr create(const QnResourcePtr& resource);
    ResourceNode(const QnResourcePtr& resource);

    virtual QVariant data(int column, int role) const override;

private:
    QnResourcePtr m_resource;
};

} // namespace desktop
} // namespace client
} // namespace nx
