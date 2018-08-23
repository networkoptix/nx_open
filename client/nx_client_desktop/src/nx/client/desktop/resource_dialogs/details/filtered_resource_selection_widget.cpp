#include "filtered_resource_selection_widget.h"

#include "ui_filtered_resource_selection_widget.h"

#include <nx/client/desktop/node_view/node_view/sorting/node_view_base_sort_model.h>
#include <nx/client/desktop/node_view/resource_node_view/resource_selection_node_view.h>
#include <nx/client/desktop/common/widgets/snapped_scroll_bar.h>

namespace nx::client::desktop {

FilteredResourceSelectionWidget::FilteredResourceSelectionWidget(QWidget* parent):
    base_type(parent),
    ui(new Ui::FilteredResourceSelectionWidget())
{
    ui->setupUi(this);

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

    const auto scrollBar = new SnappedScrollBar(this);
    scrollBar->setUseItemViewPaddingWhenVisible(true);
    setVerticalScrollBar(scrollBar->proxyScrollBar());
}

FilteredResourceSelectionWidget::~FilteredResourceSelectionWidget()
{
}

node_view::ResourceSelectionNodeView* FilteredResourceSelectionWidget::resourceSelectionView()
{
    return ui->resourcesTree;
}

} // namespace nx::client::desktop

