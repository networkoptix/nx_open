// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_list_view.h"

#include <client/client_globals.h>

#include <core/resource/resource.h>

#include <ui/common/indents.h>
#include <ui/delegates/resource_item_delegate.h>
#include <ui/models/resource/resource_list_model.h>
#include <ui/models/resource/resource_list_sorted_model.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/common/widgets/snapped_scroll_bar.h>

using namespace nx::vms::client::core;
using namespace nx::vms::client::desktop;

namespace {

static constexpr int kMaximumRows = 10;
static constexpr int kRecommendedWidth = 284;

}

QnResourceListView::QnResourceListView(QWidget* parent):
    QnResourceListView(Qn::RI_WithUrl, parent)
{
}

QnResourceListView::QnResourceListView(Qn::ResourceInfoLevel infoLevel, QWidget* parent):
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
    setProperty(nx::style::Properties::kSideIndentation, QVariant::fromValue(QnIndents()));

    auto itemDelegate = new QnResourceItemDelegate(this);
    itemDelegate->setCustomInfoLevel(infoLevel);
    setItemDelegate(itemDelegate);

    if (parent)
    {
        const auto scrollBar = new SnappedScrollBar(parent->window());
        setVerticalScrollBar(scrollBar->proxyScrollBar());
    }
}

QnResourceListView::QnResourceListView(const QnResourceList& resources, QWidget* parent):
    QnResourceListView(resources, Qn::RI_WithUrl, parent)
{
}

QnResourceListView::QnResourceListView(const QnResourceList& resources,
    Options options,
    Qn::ResourceInfoLevel infoLevel,
    QWidget* parent)
    :
    QnResourceListView(infoLevel, parent)
{
    setOptions(options);
    setResources(resources);
}

QnResourceListView::QnResourceListView(
    const QnResourceList& resources, Qn::ResourceInfoLevel infoLevel, QWidget* parent)
    :
    QnResourceListView(resources, SortAsInTreeOption, infoLevel, parent)
{
}

QnResourceListView::QnResourceListView(
    const QnResourceList& resources, Options options, QWidget* parent)
    :
    QnResourceListView(resources, options, Qn::RI_WithUrl, parent)
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
    if (value.testFlag(HideStatusOption))
        modelOptions |= QnResourceListModel::HideStatusOption;
    if (value.testFlag(ServerAsHealthMonitorOption))
        modelOptions |= QnResourceListModel::ServerAsHealthMonitorOption;
    m_model->setOptions(modelOptions);

    if (value.testFlag(SortAsInTreeOption))
    {
        NX_ASSERT(!value.testFlag(SortByNameOption), "Only one sorting may be actual");
        resetSortModel(new QnResourceListSortedModel(this));
    }
    else if (value.testFlag(SortByNameOption))
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
    setProperty(nx::style::Properties::kSuppressHoverPropery, !value);
}

QnResourcePtr QnResourceListView::selectedResource() const
{
    if (!isSelectionEnabled())
        return QnResourcePtr();

    auto index = selectionModel()->currentIndex();
    return index.data(ResourceRole).value<QnResourcePtr>();
}

QnResourceListModel* QnResourceListView::model() const
{
    return m_model;
}

QSize QnResourceListView::sizeHint() const
{
    return QSize(kRecommendedWidth,
        nx::style::Metrics::kViewRowHeight * std::min(kMaximumRows, (int) resources().size()));
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
