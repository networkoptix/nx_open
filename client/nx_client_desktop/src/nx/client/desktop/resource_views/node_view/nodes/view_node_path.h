#pragma once

#include <QtCore/QList>

#include <nx/client/desktop/resource_views/node_view/nodes/view_node_fwd.h>

namespace nx {
namespace client {
namespace desktop {

class ViewNodePath
{
public:
    using Indicies = QList<int>;

    ViewNodePath(const Indicies& indicies = Indicies());

    bool isEmpty() const;

    const Indicies& indicies() const;
    void appendIndex(int index);
    int leafIndex() const;

    NodePath parent() const;

private:
    Indicies m_indicies;
};

} // namespace desktop
} // namespace client
} // namespace nx
