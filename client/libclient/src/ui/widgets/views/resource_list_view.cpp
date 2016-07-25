#include "resource_list_view.h"

#include <ui/delegates/resource_item_delegate.h>
#include <ui/models/resource/resource_list_model.h>
#include <ui/models/resource/resource_list_sorted_model.h>

QnResourceListView::QnResourceListView(QWidget* parent):
    base_type(parent),
    m_model(new QnResourceListModel(this))
{
    m_model->setReadOnly(true);

    auto sortedModel = new QnResourceListSortedModel(this);
    sortedModel->setSourceModel(m_model);

    setModel(sortedModel);
    setRootIsDecorated(false);
    setUniformRowHeights(true);
    setHeaderHidden(true);
    setFocusPolicy(Qt::NoFocus);

    auto itemDelegate = new QnResourceItemDelegate(this);
    itemDelegate->setCustomInfoLevel(Qn::RI_WithUrl);
    setItemDelegate(itemDelegate);
}

QnResourceList QnResourceListView::resources() const
{
    return m_model->resources();
}

void QnResourceListView::setResources(const QnResourceList& resources)
{
    m_model->setResources(resources);
    model()->sort(QnResourceListModel::NameColumn);
}
