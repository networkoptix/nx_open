#pragma once

#include <nx/vms/client/desktop/common/widgets/tree_view.h>
#include <nx/vms/client/desktop/resource_views/models/fake_resource_list_model.h>

namespace nx::vms::client::desktop {

class FakeResourceListView: public TreeView
{
    Q_OBJECT
    using base_type = TreeView;

public:
    explicit FakeResourceListView(
        const FakeResourceList& fakeResources,
        QWidget* parent = nullptr);

    virtual ~FakeResourceListView() override;

protected:
    virtual QSize sizeHint() const override;

private:
    FakeResourceListModel* const m_model = nullptr;
};

} // namespace nx::vms::client::desktop
