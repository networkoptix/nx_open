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

static const QnResourceListView::Options kDefaultOptions =
    []
    {
        QnResourceListView::Options result;
        result.set(QnResourceListView::SortAsInTree);
        return result;
    }();

}

QnResourceListView::QnResourceListView(QWidget* parent):
    base_type(parent),
    m_model(new QnResourceListModel(this))
{
    m_model->setReadOnly(true);

    setModel(m_model);
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

QnResourceListView::QnResourceListView(const QnResourceList& resources,
    Options options,
    QWidget* parent)
    :
    QnResourceListView(parent)
{
    setOptions(options);
    setResources(resources);
}

QnResourceListView::QnResourceListView(const QnResourceList& resources, QWidget* parent):
    QnResourceListView(resources, kDefaultOptions, parent)
{
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

QnResourceListView::Options QnResourceListView::options() const
{
    return m_options;
}

void QnResourceListView::setOptions(Options value)
{
    if (m_options == value)
        return;

    m_options = value;

    QnResourceListModel::Options modelOptions;
    if (value.test(Option::HideStatus))
        modelOptions.set(QnResourceListModel::HideStatus);
    if (value.test(Option::ServerAsHealthMonitor))
        modelOptions.set(QnResourceListModel::ServerAsHealthMonitor);
    m_model->setOptions(modelOptions);

    if (value.test(Option::SortAsInTree))
    {
        NX_EXPECT(!value.test(Option::SortByName), "Only one sorting may be actual");
        resetSortModel(new QnResourceListSortedModel(this));
    }
    else if (value.test(Option::SortByName))
    {
        auto model = new QSortFilterProxyModel(this);
        model->setSortCaseSensitivity(Qt::CaseInsensitive);
        resetSortModel(model);
    }
    else if (m_sortModel)
    {
        resetSortModel(nullptr);
    }
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

void QnResourceListView::resetSortModel(QSortFilterProxyModel* model)
{
    setModel(nullptr);
    delete m_sortModel;
    m_sortModel = model;
    if (m_sortModel)
    {
        m_sortModel->setSourceModel(m_model);
        m_sortModel->sort(QnResourceListModel::NameColumn);
        setModel(m_sortModel);
    }
    else
    {
        setModel(m_model);
    }
}
