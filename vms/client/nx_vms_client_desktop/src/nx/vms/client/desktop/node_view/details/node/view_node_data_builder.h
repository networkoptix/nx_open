#pragma once

#include "view_node_fwd.h"
#include "view_node_data.h"

namespace nx::vms::client::desktop {
namespace node_view {
namespace details {

/**
 * Helper class for convenient filling node data.
 */
class NX_VMS_DESKTOP_CLIENT_API ViewNodeDataBuilder
{
public:
    explicit ViewNodeDataBuilder();
    explicit ViewNodeDataBuilder(ViewNodeData& data);
    ~ViewNodeDataBuilder();

    ViewNodeDataBuilder& separator();

    ViewNodeDataBuilder& withText(int column, const QString& value);
    ViewNodeDataBuilder& withCheckedState(int column, Qt::CheckState value);
    ViewNodeDataBuilder& withCheckedState(const ColumnSet& columns, Qt::CheckState value);
    ViewNodeDataBuilder& withCheckedState(int column, const OptionalCheckedState& value);
    ViewNodeDataBuilder& withIcon(int column, const QIcon& value);
    /**
    * Assuming that the sort order is ascending, nodes with lesser group sort order property will go
    *     before nodes with greater group sort order property regardless any other contents.
    *     Group sort order may be either positive or negative, default value is 0.
    *
    * @param value Custom group sort order.
    */
    ViewNodeDataBuilder& withGroupSortOrder(int value);
    ViewNodeDataBuilder& withExpanded(bool value);

    ViewNodeDataBuilder& withData(int column, int role, const QVariant& value);
    ViewNodeDataBuilder& withCommonData(int role, const QVariant& value);

    ViewNodeDataBuilder& withFlag(int column, Qt::ItemFlag flag, bool value = true);
    ViewNodeDataBuilder& withFlags(int column, Qt::ItemFlags flags);

    ViewNodeData data() const;

private:
    const bool m_outerData;
    QScopedPointer<ViewNodeData> m_data;
};

} // namespace details
} // namespace node_view
} // namespace nx::vms::client::desktop
