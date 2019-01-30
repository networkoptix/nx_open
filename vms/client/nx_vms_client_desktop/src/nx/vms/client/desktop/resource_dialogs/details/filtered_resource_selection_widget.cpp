#include "filtered_resource_selection_widget.h"

#include "ui_filtered_resource_selection_widget.h"

#include <ui/style/custom_style.h>
#include <nx/vms/client/desktop/node_view/node_view/sorting_filtering/node_view_base_sort_model.h>
#include <nx/vms/client/desktop/node_view/resource_node_view/resource_selection_node_view.h>
#include <nx/vms/client/desktop/common/widgets/snapped_scroll_bar.h>

namespace nx::vms::client::desktop {

FilteredResourceSelectionWidget::FilteredResourceSelectionWidget(QWidget* parent):
    base_type(parent),
    ui(new Ui::FilteredResourceSelectionWidget())
{
    ui->setupUi(this);

    ui->detailsArea->setVisible(false);
    clearInvalidMessage();

    const auto stackedWidget =  ui->stackedWidget;
    stackedWidget->setCurrentWidget(ui->resourcesPage);

    connect(ui->searchLineEdit, &SearchLineEdit::textChanged, this,
        [this, stackedWidget](const QString& text)
        {
            using SortModel = node_view::NodeViewBaseSortModel;
            const auto viewModel = static_cast<SortModel*>(ui->resourcesTree->model());
            viewModel->setFilter(text, SortModel::LeafNodeFilterScope);
            const auto page = viewModel->rowCount(QModelIndex())
                ? ui->resourcesPage
                : ui->notificationPage;
            stackedWidget->setCurrentWidget(page);
        });

    const auto scrollBar = new SnappedScrollBar(ui->holder);
    scrollBar->setUseItemViewPaddingWhenVisible(true);
    ui->scrollArea->setVerticalScrollBar(scrollBar->proxyScrollBar());
}

FilteredResourceSelectionWidget::~FilteredResourceSelectionWidget()
{
}

node_view::ResourceSelectionNodeView* FilteredResourceSelectionWidget::view()
{
    return ui->resourcesTree;
}

QString FilteredResourceSelectionWidget::invalidMessage() const
{
    return ui->alertBar->text();
}

void FilteredResourceSelectionWidget::clearInvalidMessage()
{
    setInvalidMessage(QString());
}

void FilteredResourceSelectionWidget::setInvalidMessage(const QString& text)
{
    ui->alertBar->setText(text);
}

bool FilteredResourceSelectionWidget::detailsVisible() const
{
    return ui->detailsArea->isVisible();
}

void FilteredResourceSelectionWidget::setDetailsVisible(bool visible)
{
    if (visible != detailsVisible())
        ui->detailsArea->setVisible(visible);
}

} // namespace nx::vms::client::desktop

