#pragma once

#include <core/resource/resource_fwd.h>
#include "../selection_node_view/selection_node_view.h"

namespace nx {
namespace client {
namespace desktop {
namespace node_view {

class ResourceNodeView: public SelectionNodeView
{
    Q_OBJECT
    using base_type = SelectionNodeView;

public:
    ResourceNodeView(QWidget* parent = nullptr);
    virtual ~ResourceNodeView() override;

    virtual void setupHeader() override;

    void setSelectedResources(const QnResourceList& resources, bool value);

private:
    struct Private;
    const QScopedPointer<Private> d;
};

} // namespace node_view
} // namespace desktop
} // namespace client
} // namespace nx
