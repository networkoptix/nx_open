#pragma once

#include <nx/client/desktop/resource_views/node_view/node_view_state.h>
#include <nx/client/desktop/resource_views/node_view/node_view_state_patch.h>
#include <nx/client/desktop/resource_views/node_view/nodes/view_node.h>

namespace nx {
namespace client {
namespace desktop {

class NodeViewStore: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    NodeViewStore(QObject* parent = nullptr);
    virtual ~NodeViewStore() override;

    const NodeViewState& state() const;

    void setNodeChecked(
        const ViewNodePath& path,
        Qt::CheckState checkedState);

    void applyPatch(const NodeViewStatePatch& state);

signals:
    void patchApplied(const NodeViewStatePatch& patch);

private:
    struct Private;
    const QScopedPointer<Private> d;
};

} // namespace desktop
} // namespace client
} // namespace nx
