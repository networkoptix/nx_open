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
    struct PathInternal;
    using Path = std::shared_ptr<PathInternal>;

    static NodePtr create(const NodeList& children = NodeList(), bool checkable = false);
    virtual ~ViewNode();

    int childrenCount() const;
    const NodeList& children() const;

    NodePtr nodeAt(int index) const;

    NodePtr nodeAt(const Path& path);
    Path path(); //< TODO: think abount const

    // TODO: refactor this O(N) complexity
    int indexOf(const NodePtr& node) const;

    virtual QVariant data(int column, int role) const;

    virtual Qt::ItemFlags flags(int column) const;

    NodePtr parent() const;

    bool checkable() const;
    Qt::CheckState checkedState() const;
    void setCheckedState(Qt::CheckState value);

    void clone() const;

protected:
    ViewNode(bool checkable);

    void setParent(const WeakNodePtr& value);

    void addNode(const NodePtr& node);

private:
    WeakNodePtr currentSharedNode();

private:
    const bool m_checkable = false;
    Qt::CheckState m_checkedState = Qt::Unchecked;
    WeakNodePtr m_parent;
    NodeList m_nodes;
};

} // namespace desktop
} // namespace client
} // namespace nx
