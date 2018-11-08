#include "view_node_path.h"

namespace nx::vms::client::desktop {
namespace node_view {
namespace details {

ViewNodePath::ViewNodePath(const ViewNodePath& other):
    m_indices(other.indices())
{
}

ViewNodePath::ViewNodePath(const Indices& indices):
    m_indices(indices)
{
}

bool ViewNodePath::isEmpty() const
{
    return m_indices.isEmpty();
}

const ViewNodePath::Indices& ViewNodePath::indices() const
{
    return m_indices;
}

void ViewNodePath::appendIndex(int index)
{
    m_indices.append(index);
}

int ViewNodePath::lastIndex() const
{
    return m_indices.isEmpty()
        ? 0 //< Root node is always single and has '0' index.
        : m_indices.last();
}

ViewNodePath ViewNodePath::parentPath() const
{
    if (m_indices.isEmpty())
        return ViewNodePath();

    auto indices = m_indices;
    indices.pop_back();
    return ViewNodePath(indices);
}

bool operator==(const ViewNodePath& left, const ViewNodePath& right)
{
    return left.indices() == right.indices();
}

} // namespace details
} // namespace node_view
} // namespace nx::vms::client::desktop

