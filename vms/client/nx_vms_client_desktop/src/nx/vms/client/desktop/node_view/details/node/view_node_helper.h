#pragma once

#include "view_node_fwd.h"

namespace nx::vms::client::desktop {
namespace node_view {
namespace details {

class NX_VMS_CLIENT_DESKTOP_API ViewNodeHelper
{
public:
    static int makeUserActionRole(int initialRole, bool isUserAction = true);

    static NodePtr nodeFromIndex(const QModelIndex& index);

    using ForEachNodeCallback = std::function<void (const NodePtr& node)>;
    static void forEachLeaf(const NodePtr& root, const ForEachNodeCallback& callback);
    static void forEachNode(const NodePtr& root, const ForEachNodeCallback& callback);

    static NodePtr createSimpleNode(
        const QString& caption,
        const NodeList& children,
        int checkableColumn = -1,
        int groupSortOrder = 0);

    static NodePtr createSimpleNode(
        const QString& caption,
        int checkableColumn = -1,
        int groupSortOrder = 0);

    static NodePtr createSeparatorNode(int groupSortOrder = 0);


public:
    ViewNodeHelper(const QModelIndex& index);
    ViewNodeHelper(const ViewNodeData& data);
    ViewNodeHelper(const NodePtr& node);

    bool expanded() const;

    bool isSeparator() const;

    bool hoverable() const;

    bool checkable(int column) const;

    Qt::CheckState userCheckedState(int column) const;

    Qt::CheckState checkedState(int column, bool isUserAction = false);

    qreal progressValue(int column) const;

    QString text(int column) const;

    bool useSwitchStyleForCheckbox(int column) const;

    /**
    * Assuming that the sort order is ascending, nodes with lesser group sort order property will go
    *     before nodes with greater group sort order property regardless any other contents.
    *     Group sort order may be either positive or negative, default value is 0.
    */
    int groupSortOrder() const;

private:
    const ViewNodeData* const m_data;
};

} // namespace details
} // namespace node_view
} // namespace nx::vms::client::desktop
