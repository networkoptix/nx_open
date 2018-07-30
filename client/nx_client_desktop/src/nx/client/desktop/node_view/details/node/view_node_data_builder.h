#pragma once

#include "view_node_fwd.h"
#include "view_node_data.h"

namespace nx {
namespace client {
namespace desktop {
namespace node_view {
namespace details {

class ViewNodeDataBuilder
{
public:
    explicit ViewNodeDataBuilder();
    explicit ViewNodeDataBuilder(ViewNodeData& data);
    ~ViewNodeDataBuilder();

    ViewNodeDataBuilder& separator();

    ViewNodeDataBuilder& withText(int column, const QString& value);
    ViewNodeDataBuilder& withCheckedState(int column, Qt::CheckState value);
    ViewNodeDataBuilder& withCheckedState(const ColumnsSet& columns, Qt::CheckState value);
    ViewNodeDataBuilder& withCheckedState(int column, const OptionalCheckedState& value);
    ViewNodeDataBuilder& withIcon(int column, const QIcon& value);
    ViewNodeDataBuilder& withSiblingGroup(int value);
    ViewNodeDataBuilder& withExpanded(bool value);

    ViewNodeDataBuilder& withFlags(int column, Qt::ItemFlags flags);

    ViewNodeData data() const;

private:
    const bool m_outerData;
    QScopedPointer<ViewNodeData> m_data;
};

} // namespace details
} // namespace node_view
} // namespace desktop
} // namespace client
} // namespace nx
