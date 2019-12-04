#pragma once

#include <QtCore/QScopedPointer>

#include "../node_view/tree_node_view.h"

namespace nx::vms::client::desktop {
namespace node_view {

class ResourceNodeView: public TreeNodeView
{
    Q_OBJECT
    using base_type = TreeNodeView;

public:
    ResourceNodeView(QWidget* parent = nullptr);
    virtual ~ResourceNodeView();

private:
    struct Private;
    const QScopedPointer<Private> d;
};

} // namespace node_view
} // namespace nx::vms::client::desktop
