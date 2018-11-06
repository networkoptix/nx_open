#pragma once

#include <QtCore/QScopedPointer>

#include "../node_view/node_view.h"

namespace nx::vms::client::desktop {
namespace node_view {

class ResourceNodeView: public NodeView
{
    Q_OBJECT
    using base_type = NodeView;

public:
    ResourceNodeView(QWidget* parent = nullptr);
    virtual ~ResourceNodeView();

private:
    struct Private;
    const QScopedPointer<Private> d;
};

} // namespace node_view
} // namespace nx::vms::client::desktop
