#pragma once

#include <nx/client/desktop/resource_views/node_view/nodes/view_node_data.h>

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

    ViewNodeDataBuilder& withText(const QString& value);
    ViewNodeDataBuilder& withCheckedState(Qt::CheckState value);
    ViewNodeDataBuilder& withCheckable(bool checkable = true, Qt::CheckState state = Qt::Unchecked);
    ViewNodeDataBuilder& withIcon(const QIcon& value);
    ViewNodeDataBuilder& withSiblingGroup(int value);

    ViewNodeDataBuilder& withCustomData(int column, int role, const QVariant& value);

    ViewNodeDataBuilder& withFlags(int column, Qt::ItemFlags flags);

    const ViewNodeData& data() const;

private:
    const bool m_outerData;
    QScopedPointer<ViewNodeData> m_data;
};

} // namespace desktop
} // namespace client
} // namespace nx
