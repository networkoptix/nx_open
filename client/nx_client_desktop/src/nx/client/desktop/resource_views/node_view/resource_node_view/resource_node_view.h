#pragma once

#include <nx/client/desktop/resource_views/node_view/selection_node_view.h>

namespace nx {
namespace client {
namespace desktop {

class ResourceNodeView: public node_view::SelectionNodeView
{
    Q_OBJECT
    using base_type = SelectionNodeView;

public:
    ResourceNodeView(QWidget* parent = nullptr);
    virtual ~ResourceNodeView() override;

private:
    struct Private;
    const QScopedPointer<Private> d;
};

} // namespace desktop
} // namespace client
} // namespace nx
