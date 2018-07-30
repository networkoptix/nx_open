#pragma once

#include "../node_view/node_view.h"
#include "../details/node/view_node_fwd.h"

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
        const details::ColumnsSet& selectionColumns,
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
