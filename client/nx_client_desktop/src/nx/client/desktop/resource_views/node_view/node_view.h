#pragma once

#include <utils/common/connective.h>
#include <nx/client/desktop/common/widgets/tree_view.h>

class QSortFilterProxyModel;

namespace nx {
namespace client {
namespace desktop {

namespace details
{

class NodeViewModel;
class NodeViewStore;

} // namespace details

class NodeViewState;
class NodeViewStatePatch;

class NodeView: public Connective<TreeView>
{
    Q_OBJECT
    using base_type = Connective<TreeView>;

public:
    NodeView(QWidget* parent = nullptr);
    virtual ~NodeView() override;

    const NodeViewState& state() const;

    void applyPatch(const NodeViewStatePatch& patch);

    void setProxyModel(QSortFilterProxyModel* proxy);

protected:
    const details::NodeViewStore* store() const;

    details::NodeViewModel* sourceModel() const;

private:
    // Node view uses special model and proxy and we have to control this process.
    // Thus it is denied to set model directly. Please use setProxyModel to set any other proxy.
    using base_type::setModel;

private:
    struct Private;
    const QScopedPointer<Private> d;
};

} // namespace desktop
} // namespace client
} // namespace nx
