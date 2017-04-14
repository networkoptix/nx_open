#include "resource_list_view.h"

#include <client/client_globals.h>

#include <core/resource/resource.h>

#include <ui/common/indents.h>
#include <ui/delegates/resource_item_delegate.h>
#include <ui/models/resource/resource_list_model.h>
#include <ui/models/resource/resource_list_sorted_model.h>
#include <ui/style/helper.h>
#include <ui/widgets/common/snapped_scrollbar.h>

namespace {

static const int kMaximumRows = 10;
static const int kRecommendedWidth = 284;

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
    setSelectionEnabled(false);
    setProperty(style::Properties::kSideIndentation, qVariantFromValue(QnIndents()));

    auto itemDelegate = new QnResourceItemDelegate(this);
    itemDelegate->setCustomInfoLevel(Qn::RI_WithUrl);
    setItemDelegate(itemDelegate);

    if (parent)
    {
        const auto scrollBar = new QnSnappedScrollBar(parent->window());
        setVerticalScrollBar(scrollBar->proxyScrollBar());
    }
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

bool QnResourceListView::isSelectionEnabled() const
{
    return selectionMode() != QAbstractItemView::NoSelection;
}

void QnResourceListView::setSelectionEnabled(bool value)
{
    setSelectionMode(value ? QAbstractItemView::SingleSelection : QAbstractItemView::NoSelection);
    setProperty(style::Properties::kSuppressHoverPropery, !value);
}

QnResourcePtr QnResourceListView::selectedResource() const
{
    if (!isSelectionEnabled())
        return QnResourcePtr();

    auto index = selectionModel()->currentIndex();
    return index.data(Qn::ResourceRole).value<QnResourcePtr>();
}

QSize QnResourceListView::sizeHint() const
{
    return QSize(kRecommendedWidth, style::Metrics::kViewRowHeight * std::min(kMaximumRows, resources().size()));
}
