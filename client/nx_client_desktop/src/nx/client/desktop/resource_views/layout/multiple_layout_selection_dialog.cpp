#include "multiple_layout_selection_dialog.h"
#include "ui_multiple_layout_selection_dialog.h"

#include <nx/client/desktop/resource_views/layout/layout_selection_model.h>
#include <nx/client/desktop/resource_views/layout/layout_selection_dialog_store.h>

#include <common/common_module.h>
#include <client_core/client_core_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/layout_resource.h>

#include <ui/delegates/resource_item_delegate.h>

namespace nx {
namespace client {
namespace desktop {

struct MultipleLayoutSelectionDialog::Private
{
    Private(MultipleLayoutSelectionDialog* owner);

    void updateState();

    MultipleLayoutSelectionDialog* const owner;
    LayoutSelectionDialogStore store;
    LayoutsModel model;
};

MultipleLayoutSelectionDialog::Private::Private(MultipleLayoutSelectionDialog* owner):
    owner(owner)
{
    const auto pool = qnClientCoreModule->commonModule()->resourcePool();
    store.setResources(pool->getResources());

    const auto loadStateHandler = [this]() { model.loadState(store.state()); };
    connect(&store, &LayoutSelectionDialogStore::stateChanged, &model, loadStateHandler);
    loadStateHandler();
}

//-------------------------------------------------------------------------------------------------

MultipleLayoutSelectionDialog::MultipleLayoutSelectionDialog(QWidget* parent):
    base_type(parent),
    d(new Private(this)),
    ui(new Ui::MultipleLayoutSelectionDialog)
{
    ui->setupUi(this);

    const auto tree = ui->layoutsTree;
    tree->setModel(&d->model);

    const auto delegate = new QnResourceItemDelegate(tree);
    tree->setItemDelegateForColumn(LayoutsModel::Columns::Name, delegate);
    tree->expandAll();
}

MultipleLayoutSelectionDialog::~MultipleLayoutSelectionDialog()
{
}

} // namespace desktop
} // namespace client
} // namespace nx
