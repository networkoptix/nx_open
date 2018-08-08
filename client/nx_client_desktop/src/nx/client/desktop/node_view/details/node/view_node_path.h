#pragma once

#include <QtCore/QList>

namespace nx {
namespace client {
namespace desktop {
namespace node_view {
namespace details {

/**
 * Relative path from one item to another. Consist of indexes for child items for each node.
 */
class ViewNodePath
{
public:
    using Indices = QList<int>;

    ViewNodePath(const ViewNodePath& other);
    ViewNodePath(const Indices& indices = Indices());

    /**
     * Checks if there are no indices and path points to the applied item.
     */
    bool isEmpty() const;

    const Indices& indices() const;

    void appendIndex(int index);

    /**
     * Returns index of last node.
     */
    int lastIndex() const;

    /**
     * Returns path to parent node.
     */
    ViewNodePath parentPath() const;

private:
    Indices m_indices;
};

bool operator==(const ViewNodePath& left, const ViewNodePath& right);

} // namespace details
} // namespace node_view
} // namespace desktop
} // namespace client
} // namespace nx
