#pragma once

#include <ui/widgets/common/tree_view.h>
#include <nx/client/desktop/resource_views/models/fake_resource_list_model.h>

namespace nx {
namespace client {
namespace desktop {

class FakeResourceListView: public QnTreeView
{
    Q_OBJECT
    using base_type = QnTreeView;

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

} // namespace desktop
} // namespace client
} // namespace nx
