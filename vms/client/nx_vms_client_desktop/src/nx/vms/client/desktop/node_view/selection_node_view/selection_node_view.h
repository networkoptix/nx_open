#pragma once

#include "../node_view/node_view.h"
#include "../details/node/view_node_fwd.h"
#include "../details/node/view_node_path.h"

namespace nx::vms::client::desktop {
namespace node_view {

/**
 * Supports selection functionality for specified columns. If parent node is checkable then its
 * checked state represents cumulative state of checkable children nodes.
 * It is supposed that all initial nodes should be added unchecked. Then you can use
 * setSelectedNodes function to mark nodes selected.
 * View does not support automatic selection state calculation for nodes added/removed after
 * initialization.
 */
class SelectionNodeView: public NodeView
{
    Q_OBJECT
    using base_type = NodeView;

public:
    /**
     * Constructs view.
     * @param columnsCount Overall columns count.
     * @param selectionColumns List of columns which will be treated as selection markers.
     *     All selection columns calculate their selection (checked) state automatically.
     * @param parent Parent widget.
     */
    SelectionNodeView(
        int columnsCount,
        const details::ColumnSet& selectionColumns,
        QWidget* parent = nullptr);

    virtual ~SelectionNodeView() override;

    /**
     * Set selected state for nodes by specified paths.
     * Please use this function for selection instead of direct patch applying.
     * Otherwise consistent state is not guaranteed.
     */
    void setSelectedNodes(const details::PathList& paths, bool value);

    const details::ColumnSet& selectionColumns() const;

signals:
    void selectionChanged(const details::ViewNodePath& path, Qt::CheckState checkedState);

protected:
    virtual void handleDataChangeRequest(
        const QModelIndex& index,
        const QVariant& value,
        int role) override;

private:
    struct Private;
    const QScopedPointer<Private> d;
};

} // namespace node_view
} // namespace nx::vms::client::desktop
