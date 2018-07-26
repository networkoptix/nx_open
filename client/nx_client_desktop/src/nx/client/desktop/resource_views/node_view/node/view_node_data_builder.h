#pragma once

#include <nx/client/desktop/resource_views/node_view/node/view_node_fwd.h>
#include <nx/client/desktop/resource_views/node_view/node/view_node_data.h>

namespace nx {
namespace client {
namespace desktop {

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
    ViewNodeDataBuilder& withAllSiblingsCheckMode();

    ViewNodeDataBuilder& withFlags(int column, Qt::ItemFlags flags);

    const ViewNodeData& data() const;

private:
    const bool m_outerData;
    QScopedPointer<ViewNodeData> m_data;
};

} // namespace desktop
} // namespace client
} // namespace nx
