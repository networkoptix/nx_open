#pragma once

#include "../selection_node_view/selection_node_view.h"

namespace nx {
namespace client {
namespace desktop {
namespace node_view {

class ResourceNodeView: public SelectionNodeView
{
    Q_OBJECT
    using base_type = SelectionNodeView;

public:
    ResourceNodeView(QWidget* parent = nullptr);
    virtual ~ResourceNodeView() override;

    virtual void setupHeader() override;
private:
    struct Private;
    const QScopedPointer<Private> d;
};

} // namespace node_view
} // namespace desktop
} // namespace client
} // namespace nx
