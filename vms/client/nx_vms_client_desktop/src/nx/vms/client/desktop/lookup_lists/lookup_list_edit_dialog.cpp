// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "lookup_list_edit_dialog.h"

#include <QtCore/QUrl>

#include <nx/vms/client/core/analytics/analytics_taxonomy_manager.h>
#include <nx/vms/client/core/analytics/taxonomy/state_view.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/system_context.h>

#include "lookup_list_model.h"

namespace nx::vms::client::desktop {

void LookupListEditDialog::registerQmlTypes()
{
    qmlRegisterType<LookupListModel>("nx.vms.client.desktop", 1, 0, "LookupListModel");
}

LookupListEditDialog::LookupListEditDialog(SystemContext* systemContext,
    core::analytics::taxonomy::StateView* taxonomy,
    LookupListModel* sourceModel,
    QWidget* parent):
    base_type(appContext()->qmlEngine(),
        QUrl("Nx/LookupLists/EditLookupListDialog.qml"),
        /*initialProperties*/
        {{"taxonomy", QVariant::fromValue(taxonomy)},
            {"sourceModel", QVariant::fromValue(sourceModel)},
            {"deletionIsAllowed", false}},
        parent),
    SystemContextAware(systemContext)
{
}

LookupListEditDialog::~LookupListEditDialog()
{
    QmlProperty<QObject*>(rootObjectHolder(), "taxonomy") = nullptr;
    QmlProperty<QObject*>(rootObjectHolder(), "sourceModel") = nullptr;
}

api::LookupListData LookupListEditDialog::getLookupListData()
{
    const auto resultModel =
        QmlProperty<LookupListModel*>(rootObjectHolder(), "viewModel").value();
    if (!NX_ASSERT(resultModel))
        return {};

    return resultModel->rawData();
}

} // namespace nx::vms::client::desktop
