#include "resource_list_view.h"

#include <ui/delegates/resource_item_delegate.h>
#include <ui/models/resource/resource_list_model.h>
#include <ui/models/resource/resource_list_sorted_model.h>
#include <ui/style/helper.h>

namespace {

static const int kMaximumRows = 10;
static const int kRecommendedWidth = 400;

}

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

QnResourceListView::QnResourceListView(const QnResourceList& resources, QWidget* parent):
    QnResourceListView(parent)
{
    setResources(resources);
}

QnResourceListView::QnResourceListView(const QnResourceList& resources, bool ignoreStatus,
    QWidget* parent)
    :
    QnResourceListView(resources, parent)
{
    setSimplified(ignoreStatus);
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

bool QnResourceListView::isSimplified() const
{
    return m_model->isSimplified();
}

void QnResourceListView::setSimplified(bool value)
{
    m_model->setSimplified(value);
}

QSize QnResourceListView::sizeHint() const
{
    return QSize(kRecommendedWidth, style::Metrics::kViewRowHeight * std::min(kMaximumRows, resources().size()));
}
