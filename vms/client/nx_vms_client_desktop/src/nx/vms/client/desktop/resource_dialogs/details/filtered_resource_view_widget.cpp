// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "filtered_resource_view_widget.h"

#include "ui_filtered_resource_view_widget.h"

#include <QtWidgets/QAction>

#include <core/resource/resource.h>
#include <utils/common/event_processors.h>

#include <ui/common/indents.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/common/widgets/snapped_scroll_bar.h>
#include <nx/vms/client/desktop/resource_dialogs/filtering/filtered_resource_proxy_model.h>
#include <nx/vms/client/desktop/common/models/item_model_algorithm.h>
#include <nx/vms/client/desktop/common/utils/item_view_hover_tracker.h>
#include <nx/vms/client/desktop/resource_dialogs/details/resource_dialog_item_selection_model.h>

namespace {

static const auto kViewItemIndents = QnIndents(
    nx::style::Metrics::kDefaultTopLevelMargin,
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

    ui->headerBar->setVisible(false);
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
    setSideIndentation(kViewItemIndents);

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
    QModelIndexList result;

    const auto selectedProxyIndexes = treeView()->selectionModel()->selectedIndexes();
    for (const auto proxyIndex: selectedProxyIndexes)
        result.append(m_filterProxyModel->mapToSource(proxyIndex));

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

void FilteredResourceViewWidget::setItemViewEnabled(bool enabled)
{
    ui->treeView->setEnabled(enabled);
    ui->searchLineEdit->setEnabled(enabled);
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

    ui->headerBar->setVisible(m_headerWidget != nullptr);
}

bool FilteredResourceViewWidget::headerWidgetVisible() const
{
    if (!NX_ASSERT(m_headerWidget, "Header widget hasn't been set"));
        return false;

    return ui->headerBar->isVisible();
}

void FilteredResourceViewWidget::setHeaderWidgetVisible(bool visible)
{
    if (!NX_ASSERT(m_headerWidget, "Header widget hasn't been set"))
        return;

    ui->headerBar->setVisible(visible);
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
    if (!NX_ASSERT(m_footerWidget, "Footer widget hasn't been set"));
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
            const auto filterString = m_filterProxyModel->filterRegExp().pattern();
            const auto newFilterString = text.trimmed();
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

            m_filterProxyModel->setFilterWildcard(text);

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
    connect(treeView(), &QAbstractItemView::entered, this,
        [this](const QModelIndex& proxyIndex)
        {
            emit itemHovered(m_filterProxyModel->mapToSource(proxyIndex));
        });

    connect(treeView(), &QAbstractItemView::viewportEntered, this,
        [this]
        {
            emit itemHovered(QModelIndex());
        });

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
        treeView()->resetIndentation();
    else
        treeView()->setIndentation(0);
}

void FilteredResourceViewWidget::setSideIndentation(const QnIndents& indents)
{
    treeView()->setProperty(style::Properties::kSideIndentation, QVariant::fromValue(indents));
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
    if (!m_filterProxyModel->sourceModel()
        || !sourceIndex.isValid()
        || m_filterProxyModel->sourceModel() != sourceIndex.model())
    {
        return QModelIndex();
    }

    return m_filterProxyModel->mapFromSource(sourceIndex);
}

void FilteredResourceViewWidget::leaveEvent(QEvent* event)
{
    base_type::leaveEvent(event);
    emit itemHovered(QModelIndex());
}

} // namespace nx::vms::client::desktop
