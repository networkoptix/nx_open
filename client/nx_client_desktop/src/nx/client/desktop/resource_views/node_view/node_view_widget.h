#pragma once

#include <utils/common/connective.h>
#include <nx/client/desktop/common/widgets/tree_view.h>

class QSortFilterProxyModel;

namespace nx {
namespace client {
namespace desktop {

class NodeViewState;
class NodeViewStatePatch;

class NodeViewWidget: public Connective<TreeView>
{
    Q_OBJECT
    using base_type = Connective<TreeView>;

public:
    NodeViewWidget(QWidget* parent = nullptr);
    virtual ~NodeViewWidget() override;

    const NodeViewState& state() const;

    void applyPatch(const NodeViewStatePatch& patch);

    void setProxyModel(QSortFilterProxyModel* proxy);

private:
    using base_type::setModel;

private:
    struct Private;
    const QScopedPointer<Private> d;
};

} // namespace desktop
} // namespace client
} // namespace nx
