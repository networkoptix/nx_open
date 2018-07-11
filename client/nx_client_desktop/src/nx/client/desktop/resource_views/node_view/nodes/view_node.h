#pragma once

#include <nx/utils/uuid.h>
#include <nx/client/desktop/resource_views/node_view/nodes/view_node_fwd.h>

namespace nx {
namespace client {
namespace desktop {

class ViewNodePath;

class ViewNode: public QEnableSharedFromThis<ViewNode>
{
    struct PathInternal;

public:
    struct Data
    {
        using Column = int;
        using Role = int;

        using ColumnFlagHash = QHash<Column, Qt::ItemFlags>;
        using RoleValueHash = QHash<Role, QVariant>;
        using ColumnDataHash = QHash<Column, RoleValueHash>;

        ColumnFlagHash flags;
        ColumnDataHash data;
    };

public:
    static NodePtr create(const Data& data);
    static NodePtr create(const NodeList& children);
    static NodePtr create(const Data& data, const NodeList& children);

    ~ViewNode();

    bool isLeaf() const;
    int childrenCount() const;
    const NodeList& children() const;

    void addChild(const NodePtr& child);

    NodePtr nodeAt(int index);
    NodePtr nodeAt(const ViewNodePath& path);
    ViewNodePath path() const;

    int indexOf(const ConstNodePtr& node) const;

    bool hasData(int column, int role) const;
    QVariant data(int column, int role) const;

    Qt::ItemFlags flags(int column) const;

    NodePtr parent() const;

    bool checkable() const;

    const Data& nodeData() const;
    void setNodeData(const Data& data);
    void applyNodeData(const Data& data);

private:
    ViewNode(const Data& data);

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

