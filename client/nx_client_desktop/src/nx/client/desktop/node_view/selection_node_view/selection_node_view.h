#pragma once

#include "../node_view/node_view.h"
#include "../details/node/view_node_fwd.h"
#include "../details/node/view_node_path.h"

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

    /**
     * Set selected state for nodes by specified paths.
     * Please use this function for selection instead of direct patch applying.
     * Otherwise consistent state is not guaranteed.
     */
    void setSelectedNodes(const details::PathList& paths, bool value);

protected:
    virtual void handleSourceModelDataChanged(
        const QModelIndex& index,
        const QVariant& value,
        int role) override;

private:
    struct Private;
    const QScopedPointer<Private> d;
};

} // namespace node_view
} // namespace desktop
} // namespace client
} // namespace nx
