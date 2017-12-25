#pragma once

#include <ui/widgets/common/tree_view.h>
#include <ui/models/resource/fake_resource_list_model.h>

class QnFakeResourceListView: public QnTreeView
{
    Q_OBJECT
    using base_type = QnTreeView;

public:
    explicit QnFakeResourceListView(
        const QnFakeResourceList& fakeResources,
        QWidget* parent = nullptr);

    ~QnFakeResourceListView();

protected:
    virtual QSize sizeHint() const override;

private:
    QnFakeResourceListModel* const m_model = nullptr;
};
