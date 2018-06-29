#pragma once

#include <nx/utils/uuid.h>
#include <nx/client/desktop/resource_views/node_view/nodes/view_node_fwd.h>

namespace nx {
namespace client {
namespace desktop {

// TODO: Add description about addNode and cosntructor
class ViewNode: public QEnableSharedFromThis<ViewNode>
{
public:
    static NodePtr create(const NodeList& children = NodeList());
    virtual ~ViewNode();

    int childrenCount() const;

    NodePtr nodeAt(int index) const;

    // TODO: refactor this O(N) complexity
    int indexOf(const NodePtr& node) const;

    virtual QVariant data(int column, int role) const;

    virtual Qt::ItemFlags flags(int column) const;

    NodePtr parent() const;

protected:
    ViewNode();

    void setParent(const WeakNodePtr& value);

    void addNode(const NodePtr& node);

private:
    WeakNodePtr parentForChildren();

private:
    WeakNodePtr m_parent;
    NodeList m_nodes;
};

} // namespace desktop
} // namespace client
} // namespace nx
