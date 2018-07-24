#pragma once

#include <nx/utils/uuid.h>
#include <nx/client/desktop/resource_views/node_view/node/view_node_fwd.h>

namespace nx {
namespace client {
namespace desktop {

class ViewNodePath;
class ViewNodeData;

class ViewNode: public QEnableSharedFromThis<ViewNode>
{
public:
    static NodePtr create(const ViewNodeData& data);
    static NodePtr create(const NodeList& children);
    static NodePtr create(const ViewNodeData& data, const NodeList& children);

    ~ViewNode();

    bool isLeaf() const;
    int childrenCount() const;
    const NodeList& children() const;

    void addChild(const NodePtr& child);

    NodePtr nodeAt(int index);
    NodePtr nodeAt(const ViewNodePath& path);
    ViewNodePath path() const;

    int indexOf(const ConstNodePtr& node) const;

    QVariant data(int column, int role) const;

    Qt::ItemFlags flags(int column) const;

    NodePtr parent() const;

    const ViewNodeData& nodeData() const;
    void applyNodeData(const ViewNodeData& data);

private:
    ViewNode(const ViewNodeData& data);

    WeakNodePtr currentSharedNode();
    ConstWeakNodePtr currentSharedNode() const;

private:
    struct Private;
    const QScopedPointer<Private> d;
};

uint qHash(const nx::client::desktop::ViewNodePath& path);

} // namespace desktop
} // namespace client
} // namespace nx

