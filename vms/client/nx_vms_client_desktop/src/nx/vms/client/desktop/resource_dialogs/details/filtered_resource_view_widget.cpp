// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "filtered_resource_view_widget.h"
#include "ui_filtered_resource_view_widget.h"

#include <algorithm>

#include <QtGui/QAction>

#include <core/resource/resource.h>
#include <nx/vms/client/desktop/common/models/item_model_algorithm.h>
#include <nx/vms/client/desktop/common/utils/item_view_hover_tracker.h>
#include <nx/vms/client/desktop/common/widgets/snapped_scroll_bar.h>
#include <nx/vms/client/desktop/resource_dialogs/details/resource_dialog_item_selection_model.h>
#include <nx/vms/client/desktop/resource_dialogs/filtering/filtered_resource_proxy_model.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <utils/common/event_processors.h>

namespace {

static constexpr auto kListViewExtraLeftIndent = 4;
static constexpr auto kTreeViewSideIndents = QnIndents(nx::style::Metrics::kDefaultTopLevelMargin);
static constexpr auto kListViewSideIndents = QnIndents(
    nx::style::Metrics::kDefaultTopLevelMargin + kListViewExtraLeftIndent,
    nx::style::Metrics::kDefaultTopLevelMargin);

static constexpr auto kNothingFoundLabelFontSize = 16;

QFont nothingFoundLabelFont()
{
    QFont result;
    result.setPixelSize(kNothingFoundLabelFontSize);
    return result;
}

} // namespace

namespace nx::vms::client::desktop {

FilteredResourceViewWidget::FilteredResourceViewWidget(QWidget* parent):
    base_type(parent),
    ui(new Ui::FilteredResourceViewWidget()),
    m_filterProxyModel(new FilteredResourceProxyModel())
{
    ui->setupUi(this);

    ui->nothingFoundLabel->setFont(nothingFoundLabelFont());

    treeView()->setModel(m_filterProxyModel.get());

    ui->headerContainer->setVisible(false);
    ui->footerContainer->setVisible(false);

    const auto scrollBar = new SnappedScrollBar(this);
    scrollBar->setUseMaximumSpace(true);
    scrollBar->setUseItemViewPaddingWhenVisible(true);
    ui->treeView->setVerticalScrollBar(scrollBar->proxyScrollBar());

    installEventHandler(scrollBar, {QEvent::Show, QEvent::Hide}, this,
        [this, scrollBar](QObject*, QEvent* event)
        {
            QMargins margins;
            if (event->type() == QEvent::Show)
                margins.setRight(scrollBar->width());
            ui->widgetLayout->setContentsMargins(margins);
            ui->widgetLayout->activate();
        });

    treeView()->setSelectionBehavior(QAbstractItemView::SelectRows);
    treeView()->setSelectionMode(QAbstractItemView::NoSelection);
    treeView()->setMouseTracking(true);
    treeView()->setProperty(style::Properties::kSideIndentation,
        QVariant::fromValue(kTreeViewSideIndents));

    setupSignals();
    setupFilter();
}

FilteredResourceViewWidget::~FilteredResourceViewWidget()
{
}

void FilteredResourceViewWidget::setSourceModel(QAbstractItemModel* model)
{
    m_filterProxyModel->setSourceModel(model);
}

QHeaderView* FilteredResourceViewWidget::treeHeaderView() const
{
    return treeView()->header();
}

QModelIndexList FilteredResourceViewWidget::selectedIndexes() const
{
    auto result = treeView()->selectionModel()->selectedIndexes();

    std::transform(result.begin(), result.end(), result.begin(),
        [this](auto& index) { return m_filterProxyModel->mapToSource(index); });

    return result;
}

QModelIndexList FilteredResourceViewWidget::selectedRows(int column) const
{
    auto result = treeView()->selectionModel()->selectedRows(column);

    std::transform(result.begin(), result.end(), result.begin(),
        [this](auto& index) { return m_filterProxyModel->mapToSource(index); });

    return result;
}

QRect FilteredResourceViewWidget::itemRect(const QModelIndex& index) const
{
    if (!index.isValid())
    {
        NX_ASSERT(false, "Invalid index");
        return QRect();
    }

    // In the viewport coordinate system.
    const auto visualRect = treeView()->visualRect(toViewIndex(index));
    const auto offset =
        treeView()->viewport()->mapTo(this, visualRect.topLeft()) - visualRect.topLeft();

    // In the this widget coordinate system.
    return visualRect.translated(offset);
}

void FilteredResourceViewWidget::clearSelection()
{
    treeView()->selectionModel()->clear();
}

void FilteredResourceViewWidget::clearFilter()
{
    ui->searchLineEdit->clear();
}

QModelIndex FilteredResourceViewWidget::currentIndex() const
{
    return m_filterProxyModel->mapToSource(treeView()->selectionModel()->currentIndex());
}

QWidget* FilteredResourceViewWidget::headerWidget() const
{
    return m_headerWidget;
}

void FilteredResourceViewWidget::setHeaderWidget(QWidget* widget)
{
    if (m_headerWidget == widget)
        return;

    if (m_headerWidget)
    {
        ui->headerContainerLayout->removeWidget(m_headerWidget);
        m_headerWidget->deleteLater();
    }

    m_headerWidget = widget;

    if (m_headerWidget)
        ui->headerContainerLayout->addWidget(widget); //< Ownership is transferred to layout.

    ui->headerContainer->setVisible(m_headerWidget != nullptr);
}

bool FilteredResourceViewWidget::headerWidgetVisible() const
{
    if (!NX_ASSERT(m_headerWidget, "Header widget hasn't been set"))
        return false;

    return ui->headerContainer->isVisible();
}

void FilteredResourceViewWidget::setHeaderWidgetVisible(bool visible)
{
    if (!NX_ASSERT(m_headerWidget, "Header widget hasn't been set"))
        return;

    ui->headerContainer->setVisible(visible);
}

QWidget* FilteredResourceViewWidget::footerWidget() const
{
    return m_footerWidget;
}

void FilteredResourceViewWidget::setFooterWidget(QWidget* widget)
{
    if (m_footerWidget == widget)
        return;

    if (m_footerWidget)
    {
        ui->footerContainerLayout->removeWidget(m_footerWidget);
        m_footerWidget->deleteLater();
    }

    m_footerWidget = widget;

    if (m_footerWidget)
        ui->footerContainerLayout->addWidget(widget); //< Ownership is transferred to layout.

    ui->footerContainer->setVisible(m_footerWidget != nullptr);
}

bool FilteredResourceViewWidget::footerWidgetVisible() const
{
    if (!NX_ASSERT(m_footerWidget, "Footer widget hasn't been set"))
        return false;

    return ui->footerContainer->isVisible();
}

void FilteredResourceViewWidget::setFooterWidgetVisible(bool visible)
{
    if (!NX_ASSERT(m_footerWidget, "Footer widget hasn't been set"))
        return;

    ui->footerContainer->setVisible(visible);
}

QString FilteredResourceViewWidget::invalidMessage() const
{
    return ui->alertBar->text();
}

void FilteredResourceViewWidget::setInvalidMessage(const QString& text)
{
    ui->alertBar->setText(text);
}

void FilteredResourceViewWidget::clearInvalidMessage()
{
    setInvalidMessage(QString());
}

QString FilteredResourceViewWidget::infoMessage() const
{
    return ui->infoBar->text();
}

void FilteredResourceViewWidget::setInfoMessage(const QString& text)
{
    ui->infoBar->setText(text);
}

void FilteredResourceViewWidget::clearInfoMessage()
{
    setInfoMessage(QString());
}

TreeView* FilteredResourceViewWidget::treeView() const
{
    return ui->treeView;
}

void FilteredResourceViewWidget::setupFilter()
{
    auto findAction = new QAction(this);
    findAction->setShortcut(QKeySequence(QKeySequence::Find));
    connect(findAction, &QAction::triggered, this,
        [this]() { ui->searchLineEdit->setFocus(); });
    addAction(findAction);

    connect(ui->searchLineEdit, &SearchLineEdit::textChanged, this,
        [this](const QString& text)
        {
            const auto filterString = m_filterProxyModel->filterRegularExpression().pattern();
            const auto newFilterString = QRegularExpression::escape(text.trimmed());
            const auto caseSensitivity = m_filterProxyModel->filterCaseSensitivity();

            if (filterString.compare(newFilterString, caseSensitivity) == 0)
                return;

            if (filterString.isEmpty() && !newFilterString.isEmpty())
            {
                const auto nonLeafSourceIndexes =
                    item_model::getNonLeafIndexes(m_filterProxyModel->sourceModel());
                for (const auto& sourceIndex: nonLeafSourceIndexes)
                {
                    if (treeView()->isExpanded(toViewIndex(sourceIndex)))
                        m_expandedSourceIndexes.append(sourceIndex);
                }
            }

            m_filterProxyModel->setFilterRegularExpression(newFilterString);

            if (filterString.isEmpty())
            {
                treeView()->collapseAll();
                for (const auto& index: m_expandedSourceIndexes)
                {
                    if (index.isValid())
                        treeView()->expand(toViewIndex(index));
                }
                m_expandedSourceIndexes.clear();
            }
            else
            {
                treeView()->expandAll();
            }
            const auto page = m_filterProxyModel->rowCount(QModelIndex())
                ? ui->treeViewPage
                : ui->nothingFoundPage;
            ui->stackedWidget->setCurrentWidget(page);
        });
}

void FilteredResourceViewWidget::setupSignals()
{
    connect(treeView(), &QAbstractItemView::clicked, this,
        [this](const QModelIndex& proxyIndex)
        {
            if (proxyIndex.flags().testFlag(Qt::ItemIsEnabled))
                emit itemClicked(m_filterProxyModel->mapToSource(proxyIndex));
        });

    connect(treeView()->selectionModel(), &QItemSelectionModel::currentChanged, this,
        [this](const QModelIndex& proxyCurrent, const QModelIndex& proxyPrevious)
        {
            emit currentItemChanged(m_filterProxyModel->mapToSource(proxyCurrent),
                m_filterProxyModel->mapToSource(proxyPrevious));
        });
}

QAbstractItemDelegate* FilteredResourceViewWidget::itemDelegate() const
{
    return treeView()->itemDelegate();
}

void FilteredResourceViewWidget::setItemDelegate(QAbstractItemDelegate* itemDelegate)
{
    treeView()->setItemDelegate(itemDelegate);
}

QAbstractItemDelegate* FilteredResourceViewWidget::itemDelegateForColumn(int column) const
{
    return treeView()->itemDelegateForColumn(column);
}

void FilteredResourceViewWidget::setItemDelegateForColumn(
    int column,
    QAbstractItemDelegate* itemDelegate)
{
    treeView()->setItemDelegateForColumn(column, itemDelegate);
}

void FilteredResourceViewWidget::setSelectionMode(QAbstractItemView::SelectionMode selectionMode)
{
    if (treeView()->selectionMode() == selectionMode)
        return;

    treeView()->setSelectionMode(selectionMode);
    switch (selectionMode)
    {
        case QAbstractItemView::NoSelection:
        case QAbstractItemView::SingleSelection:
            treeView()->setSelectionModel(new QItemSelectionModel(m_filterProxyModel.get(), this));
            break;

        case QAbstractItemView::MultiSelection:
        case QAbstractItemView::ExtendedSelection:
        case QAbstractItemView::ContiguousSelection:
            treeView()->setSelectionModel(
                new ResourceDialogItemSelectionModel(m_filterProxyModel.get(), this));
            break;

        default:
            NX_ASSERT(false, "Unexpected selection mode");
            break;
    }

    connect(treeView()->selectionModel(), &QItemSelectionModel::selectionChanged,
        this, &FilteredResourceViewWidget::selectionChanged, Qt::UniqueConnection);
}

bool FilteredResourceViewWidget::isEmpty() const
{
    return !m_filterProxyModel->sourceModel()
        || m_filterProxyModel->sourceModel()->rowCount() == 0;
}

QAbstractItemModel* FilteredResourceViewWidget::sourceModel() const
{
    return m_filterProxyModel->sourceModel();
}

void FilteredResourceViewWidget::setUseTreeIndentation(bool useTreeIndentation)
{
    const bool hasTreeIndentation = treeView()->indentation() > 0;
    if (hasTreeIndentation == useTreeIndentation)
        return;

    if (useTreeIndentation)
    {
        treeView()->resetIndentation();
        treeView()->setProperty(style::Properties::kSideIndentation,
            QVariant::fromValue(kTreeViewSideIndents));
    }
    else
    {
        treeView()->setIndentation(0);
        treeView()->setProperty(style::Properties::kSideIndentation,
            QVariant::fromValue(kListViewSideIndents));
    }
}

QnIndents FilteredResourceViewWidget::sideIndentation() const
{
    const auto sideIndentationProperty = treeView()->property(style::Properties::kSideIndentation);
    return sideIndentationProperty.isNull()
        ? QnIndents()
        : sideIndentationProperty.value<QnIndents>();
}

void FilteredResourceViewWidget::setUniformRowHeights(bool uniform)
{
    treeView()->setUniformRowHeights(uniform);
}

void FilteredResourceViewWidget::makeRequiredItemsVisible() const
{
    if (!m_visibleItemPredicate)
        return;

    QSet<QModelIndex> indexesToExpand;

    const auto allIndexes = item_model::getAllIndexes(treeView()->model());
    for (const auto& index: allIndexes)
    {
        if (!m_visibleItemPredicate(index))
            continue;

        auto parentIndex = index.parent();
        while (parentIndex.isValid())
        {
            if (!treeView()->isExpanded(parentIndex))
                indexesToExpand.insert(parentIndex);
            parentIndex = parentIndex.parent();
        }
    }

    for (const auto& index: std::as_const(indexesToExpand))
        treeView()->expand(index);
}

void FilteredResourceViewWidget::setVisibleItemPredicate(
    const VisibleItemPredicate& visibleItemPredicate)
{
    m_visibleItemPredicate = visibleItemPredicate;
}

ItemViewHoverTracker* FilteredResourceViewWidget::itemViewHoverTracker()
{
    if (!m_itemViewHoverTracker)
        m_itemViewHoverTracker = new ItemViewHoverTracker(treeView());

    return m_itemViewHoverTracker;
}

QModelIndex FilteredResourceViewWidget::toViewIndex(const QModelIndex& sourceIndex) const
{
    if (!(NX_ASSERT(m_filterProxyModel->sourceModel())))
        return {};

    if (!sourceIndex.isValid())
        return {};

    if (!NX_ASSERT(m_filterProxyModel->sourceModel() == sourceIndex.model()))
        return {};

    return m_filterProxyModel->mapFromSource(sourceIndex);
}

QModelIndex FilteredResourceViewWidget::toSourceIndex(const QModelIndex& viewIndex) const
{
    if (!(NX_ASSERT(m_filterProxyModel->sourceModel())))
        return {};

    if (!viewIndex.isValid())
        return {};

    if (!NX_ASSERT(m_filterProxyModel.get() == viewIndex.model()))
        return {};

    return m_filterProxyModel->mapToSource(viewIndex);
}

} // namespace nx::vms::client::desktop
