#pragma once

#include <nx/utils/uuid.h>
#include <nx/client/desktop/resource_views/node_view/nodes/view_node_fwd.h>

namespace nx {
namespace client {
namespace desktop {

class ViewNode: public QEnableSharedFromThis<ViewNode>
{
public:
    enum Column
    {
        NameColumn,
        CheckMarkColumn,

        ColumnCount
    };

    struct Data
    {
        using Column = int;
        using Role = int;

        using ColumnFlagHash = QHash<Column, Qt::ItemFlags>;
        using RoleValueHash = QHash<Role, QVariant>;
        using ColumnDataHash = QHash<Column, RoleValueHash>;

        bool checkable = false;
        Qt::CheckState checkedState = Qt::Unchecked;
        ColumnFlagHash flags;
        ColumnDataHash data;
    };

public:
    struct PathInternal;
    using Path = std::shared_ptr<PathInternal>;

    static NodePtr create(const Data& data);
    static NodePtr create(const NodeList& children);
    static NodePtr create(const Data& data, const NodeList& children);

    ~ViewNode();

    int childrenCount() const;
    const NodeList& children() const;

    NodePtr nodeAt(int index) const;

    NodePtr nodeAt(const Path& path);
    Path path(); //< TODO: think abount const

    int indexOf(const NodePtr& node) const;

    QVariant data(int column, int role) const;

    Qt::ItemFlags flags(int column) const;

    NodePtr parent() const;

    bool checkable() const;
    Qt::CheckState checkedState() const;

    const Data& nodeData() const;
    void setNodeData(const Data& data);

private:
    ViewNode(const Data& data);

    WeakNodePtr currentSharedNode();

private:
    struct Private;
    const QScopedPointer<Private> d;
};

} // namespace desktop
} // namespace client
} // namespace nx
