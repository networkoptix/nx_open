#pragma once

#include <nx/client/desktop/common/widgets/tree_view.h>

namespace nx {
namespace client {
namespace desktop {

class NodeViewState;

class NodeViewWidget: public TreeView
{
    Q_OBJECT
    using base_type = TreeView;

public:
    NodeViewWidget(QWidget* parent = nullptr);
    virtual ~NodeViewWidget() override;

    void setState(const NodeViewState& state);
    const NodeViewState& state() const;

private:
    struct Private;
    const QScopedPointer<Private> d;
};

} // namespace desktop
} // namespace client
} // namespace nx
