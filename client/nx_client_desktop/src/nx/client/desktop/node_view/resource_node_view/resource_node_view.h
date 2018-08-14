#pragma once

#include <core/resource/resource_fwd.h>
#include "../selection_node_view/selection_node_view.h"

class QnUuid;

namespace nx {
namespace client {
namespace desktop {
namespace node_view {

class ResourceNodeView: public SelectionNodeView
{
    Q_OBJECT
    using base_type = SelectionNodeView;

public:
    ResourceNodeView(QWidget* parent = nullptr);
    virtual ~ResourceNodeView() override;

    virtual void setupHeader() override;

    void setResourcesSelected(const details::UuidSet& resourceId, bool value);

    details::UuidSet selectedResources() const;

signals:
    void resourceSelectionChanged(const QnUuid& resourceId, Qt::CheckState checkedState);

private:
    struct Private;
    const QScopedPointer<Private> d;
};

} // namespace node_view
} // namespace desktop
} // namespace client
} // namespace nx
