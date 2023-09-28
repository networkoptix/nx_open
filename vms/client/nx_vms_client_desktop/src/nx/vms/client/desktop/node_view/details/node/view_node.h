// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "view_node_data.h"
#include "view_node_fwd.h"
#include "view_node_path.h"

namespace nx::vms::client::desktop {
namespace node_view {
namespace details {

class NX_VMS_CLIENT_DESKTOP_API ViewNode: public QEnableSharedFromThis<ViewNode>
{
public:
    static NodePtr create();
    static NodePtr create(const ViewNodeData& data);
    static NodePtr create(const NodeList& children);
    static NodePtr create(const ViewNodeData& data, const NodeList& children);

    ~ViewNode();

    bool isLeaf() const;
    bool isRoot() const;

    int childrenCount() const;
    const NodeList& children() const;

    void addChild(const NodePtr& child);
    void addChildren(const NodeList& children);

    void insertChild(int index, const NodePtr& child);
    void insertChildren(int index, const NodeList& children);

    void removeChild(int index);
    void removeChild(const NodePtr& child);

    NodePtr nodeAt(int index);
    NodePtr nodeAt(const ViewNodePath& path);
    ViewNodePath path() const;

    int indexOf(const ConstNodePtr& node) const;

    QVariant data(int column, int role) const;

    QVariant property(int id) const;

    Qt::ItemFlags flags(int column) const;

    NodePtr parent() const;

    const ViewNodeData& data() const;

    void applyNodeData(const ViewNodeData& data);

    void removeNodeData(const ColumnRoleHash& roleHash);

private:
    ViewNode(const ViewNodeData& data);

    WeakNodePtr currentSharedNode();
    ConstWeakNodePtr currentSharedNode() const;

private:
    struct Private;
    const QScopedPointer<Private> d;
};

size_t qHash(const ViewNodePath& path);

} // namespace details
} // namespace node_view
} // namespace nx::vms::client::desktop
