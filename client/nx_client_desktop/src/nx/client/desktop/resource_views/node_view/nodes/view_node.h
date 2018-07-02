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
    static NodePtr create(const NodeList& children = NodeList(), bool checkable = false);
    virtual ~ViewNode();

    int childrenCount() const;
    const NodeList& children() const;

    NodePtr nodeAt(int index) const;

    // TODO: refactor this O(N) complexity
    int indexOf(const NodePtr& node) const;

    virtual QVariant data(int column, int role) const;

    virtual Qt::ItemFlags flags(int column) const;

    NodePtr parent() const;

    bool checkable() const;
    bool checked() const;
    void setChecked(bool value);

protected:
    ViewNode(bool checkable);

    void setParent(const WeakNodePtr& value);

    void addNode(const NodePtr& node);

private:
    WeakNodePtr parentForChildren();

private:
    const bool m_checkable = false;
    bool m_checked = false;
    WeakNodePtr m_parent;
    NodeList m_nodes;
};

} // namespace desktop
} // namespace client
} // namespace nx
