#include "view_node_path.h"

namespace nx {
namespace client {
namespace desktop {

ViewNodePath::ViewNodePath(const Indicies& indicies):
    m_indicies(indicies)
{
}

bool ViewNodePath::isEmpty() const
{
    return m_indicies.isEmpty();
}

const ViewNodePath::Indicies& ViewNodePath::indicies() const
{
    return m_indicies;
}

void ViewNodePath::appendIndex(int index)
{
    m_indicies.append(index);
}

int ViewNodePath::leafIndex() const
{
    return m_indicies.isEmpty() ? -1 : m_indicies.last();
}

NodePath ViewNodePath::parent() const
{
    if (m_indicies.isEmpty())
        return NodePath(new ViewNodePath());

    auto indicies = m_indicies;
    indicies.pop_back();
    return NodePath(new ViewNodePath(indicies));
}

} // namespace desktop
} // namespace client
} // namespace nx

