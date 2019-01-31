#pragma once

#include <core/resource/resource_fwd.h>
#include "../selection_node_view/selection_node_view.h"
#include "../resource_node_view/details/base_resource_node_view.h"

class QnUuid;

namespace nx::vms::client::desktop {
namespace node_view {

class ResourceSelectionNodeView: public BaseResourceNodeView<SelectionNodeView>
{
    Q_OBJECT
    using base_type = BaseResourceNodeView<SelectionNodeView>;

public:
    enum SelectionMode
    {
        // Selects only clicked node.
        simpleSelectionMode,

        // Sometimes we have the same resource nodes in the different branches
        // simultaneously. With this mode they will be selected by click synchronously.
        selectEqualResourcesMode
    };

    ResourceSelectionNodeView(QWidget* parent = nullptr);
    virtual ~ResourceSelectionNodeView() override;

    virtual void setupHeader() override;

    void setSelectionMode(SelectionMode mode);

    /**
     * Sets selection state of all specified leaf resource nodes to the choosen one.
     */
    void setLeafResourcesSelected(const QnUuidSet& resourceId, bool select);

signals:
    void resourceSelectionChanged(const QnUuid& resourceId, Qt::CheckState checkedState);

protected:
    virtual void handleDataChangeRequest(
        const QModelIndex& index,
        const QVariant& value,
        int role) override;

private:
    struct Private;
    const QScopedPointer<Private> d;
};

} // namespace node_view
} // namespace nx::vms::client::desktop
