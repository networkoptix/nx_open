#pragma once

#include <utils/common/connective.h>
#include <nx/client/desktop/common/widgets/tree_view.h>

class QSortFilterProxyModel;

namespace nx {
namespace client {
namespace desktop {
namespace node_view {

namespace details {

class NodeViewModel;
class NodeViewStore;
class NodeViewState;
class NodeViewStatePatch;

} // namespace details

class NodeView: public Connective<TreeView>
{
    Q_OBJECT
    using base_type = Connective<TreeView>;

public:
    NodeView(int columnCount, QWidget* parent = nullptr);
    virtual ~NodeView() override;

    void setProxyModel(QSortFilterProxyModel* proxy);

    void applyPatch(const details::NodeViewStatePatch& patch);

    const details::NodeViewState& state() const;

    void setHeightToContent(bool value);

    virtual void setupHeader();

protected:
    const details::NodeViewStore& store() const;
    details::NodeViewModel& sourceModel() const;

private:
    // Node view uses special model and proxy. So we have to control this process.
    // Thus it is denied to set model directly. Please use setProxyModel to set any other proxy.
    virtual void setModel(QAbstractItemModel* model) override;

private:
    struct Private;
    const QScopedPointer<Private> d;
};

} // node_view
} // namespace desktop
} // namespace client
} // namespace nx
