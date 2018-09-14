#include "filtered_resource_selection_widget.h"

#include "ui_filtered_resource_selection_widget.h"

#include <ui/style/custom_style.h>
#include <nx/client/desktop/node_view/node_view/sorting/node_view_base_sort_model.h>
#include <nx/client/desktop/node_view/resource_node_view/resource_selection_node_view.h>
#include <nx/client/desktop/common/widgets/snapped_scroll_bar.h>

namespace nx::client::desktop {

FilteredResourceSelectionWidget::FilteredResourceSelectionWidget(QWidget* parent):
    base_type(parent),
    ui(new Ui::FilteredResourceSelectionWidget())
{
    ui->setupUi(this);

    ui->detailsArea->setVisible(false);
    setWarningStyle(ui->invalidTextLabel);
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
    scrollBar->setUseMaximumSpace(true);
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
    return ui->invalidTextLabel->text();
}

void FilteredResourceSelectionWidget::clearInvalidMessage()
{
    setInvalidMessage(QString());
}

void FilteredResourceSelectionWidget::setInvalidMessage(const QString& text)
{
    if (ui->invalidTextLabel->text() == text)
        return;

    ui->invalidTextLabel->setText(text);
    ui->invalidTextLabel->setVisible(!text.trimmed().isEmpty());
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

} // namespace nx::client::desktop

