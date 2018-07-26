#pragma once

#include <nx/client/desktop/resource_views/node_view/node_view.h>
#include <nx/client/desktop/resource_views/node_view/node/view_node_fwd.h>

namespace nx {
namespace client {
namespace desktop {
namespace node_view {

class SelectionNodeView: public NodeView
{
    Q_OBJECT
    using base_type = NodeView;

public:
    SelectionNodeView(
        int columnsCount,
        const ColumnsSet& selectionColumns,
        QWidget* parent = nullptr);

    virtual ~SelectionNodeView() override;

private:
    struct Private;
    const QScopedPointer<Private> d;
};

} // namespace node_view
} // namespace desktop
} // namespace client
} // namespace nx
