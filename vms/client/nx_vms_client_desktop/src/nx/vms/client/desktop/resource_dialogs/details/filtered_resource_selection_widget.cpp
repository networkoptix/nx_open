#include "filtered_resource_selection_widget.h"

#include "ui_filtered_resource_selection_widget.h"

#include <ui/style/custom_style.h>
#include <nx/vms/client/desktop/node_view/node_view/sorting_filtering/node_view_base_sort_model.h>
#include <nx/vms/client/desktop/node_view/resource_node_view/resource_selection_node_view.h>
#include <nx/vms/client/desktop/common/widgets/snapped_scroll_bar.h>
#include <nx/vms/client/desktop/node_view/resource_node_view/resource_node_view_constants.h>
#include <nx/vms/client/desktop/resources/search_helper.h>
#include <core/resource/resource.h>

namespace nx::vms::client::desktop {

FilteredResourceSelectionWidget::FilteredResourceSelectionWidget(QWidget* parent):
    base_type(parent),
    ui(new Ui::FilteredResourceSelectionWidget())
{
    ui->setupUi(this);

    ui->detailsArea->setVisible(false);
    clearInvalidMessage();

    using SortModel = node_view::NodeViewBaseSortModel;
    const auto viewModel = qobject_cast<SortModel*>(ui->resourcesTree->model());
    viewModel->setFilterFunctor(
        [](const QModelIndex& index, const QString& filterString) -> bool
        {
            const auto resourceData = index.data(node_view::ResourceNodeDataRole::resourceRole);
            if (resourceData.isValid())
            {
                const auto resource = resourceData.value<QnResourcePtr>();
                return resources::search_helper::matches(filterString, resource);
            }
            return index.data().toString().contains(filterString, Qt::CaseInsensitive);
        });

    const auto stackedWidget =  ui->stackedWidget;
    stackedWidget->setCurrentWidget(ui->resourcesPage);

    connect(ui->searchLineEdit, &SearchLineEdit::textChanged, this,
        [this, stackedWidget](const QString& text)
        {
            const auto viewModel = qobject_cast<SortModel*>(ui->resourcesTree->model());
            viewModel->setFilterString(text);
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

