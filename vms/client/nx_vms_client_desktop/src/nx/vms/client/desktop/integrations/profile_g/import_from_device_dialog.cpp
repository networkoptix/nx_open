// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "import_from_device_dialog.h"

#include <QtCore/QTimer>
#include <QtQuick/QQuickItem>
#include <QtWidgets/QWidget>

#include <api/server_rest_connection.h>
#include <core/resource/media_server_resource.h>
#include <nx/utils/guarded_callback.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/core/client_core_globals.h>
#include <nx/vms/client/core/utils/log_strings_format.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/menu/action_parameters.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/entity_item_model.h>
#include <nx/vms/client/desktop/resource_views/entity_resource_tree/resource_tree_entity_builder.h>
#include <nx/vms/client/desktop/resource_views/models/resource_tree_icon_decorator_model.h>
#include <nx/vms/client/desktop/system_context.h>
#include <ui/workbench/workbench_context.h>

#include "import_from_device_dialog_model.h"
#include "import_from_device_filter_model.h"

using namespace std::chrono;

namespace nx::vms::client::desktop::integrations {

using namespace entity_item_model;
using namespace entity_resource_tree;

static constexpr auto kLoadingAttemptTimeout = 3s;
static constexpr auto kUpdateTimeout = 30s;

struct ImportFromDeviceDialog::Private: public QObject
{
    ImportFromDeviceDialog* q = nullptr;

    const std::unique_ptr<entity_resource_tree::ResourceTreeEntityBuilder> treeEntityBuilder;
    AbstractEntityPtr devicesEntity;

    EntityItemModel devicesModel;
    ResourceTreeIconDecoratorModel iconDecoratorModel;
    ImportFromDeviceFilterModel filterModel;

    ImportFromDeviceDialogModel importDataModel;

    QmlProperty<QString> filter;

    QTimer m_loadingTimer;

public:
    Private(ImportFromDeviceDialog* owner);

    void requestArchiveSynchronizationStatus();
};

ImportFromDeviceDialog::Private::Private(ImportFromDeviceDialog* owner):
    q(owner),
    treeEntityBuilder(new ResourceTreeEntityBuilder(q->systemContext())),
    importDataModel(q),
    filter(q->rootObjectHolder(), "filter")
{
    treeEntityBuilder->setUser(q->systemContext()->accessController()->user());
    devicesEntity = treeEntityBuilder->createFlatCamerasListEntity();
    devicesModel.setRootEntity(devicesEntity.get());
    iconDecoratorModel.setSourceModel(&devicesModel);
    importDataModel.setSourceModel(&iconDecoratorModel);
    filterModel.setSourceModel(&importDataModel);

    connect(&m_loadingTimer, &QTimer::timeout,
        this, &ImportFromDeviceDialog::Private::requestArchiveSynchronizationStatus);

    m_loadingTimer.setSingleShot(true);
    m_loadingTimer.setInterval(kLoadingAttemptTimeout);

    requestArchiveSynchronizationStatus();
    m_loadingTimer.start();
}

void ImportFromDeviceDialog::Private::requestArchiveSynchronizationStatus()
{
    auto callback = nx::utils::guarded(this,
        [this](
            bool success,
            int /*handle*/,
            rest::ErrorOrData<nx::vms::api::RemoteArchiveSynchronizationStatusList> data)
        {
            NX_LOG_RESPONSE(this, success, data, "Received invalid archive synchronization status data.");

            if (!success)
            {
                m_loadingTimer.setInterval(kLoadingAttemptTimeout);
                m_loadingTimer.start();
                return;
            }

            m_loadingTimer.setInterval(kUpdateTimeout);
            m_loadingTimer.start();

            NX_ASSERT(data);
            importDataModel.setData(*data);
        });

    if (auto connection = q->systemContext()->connectedServerApi())
        connection->getRemoteArchiveSynchronizationStatus(std::move(callback), thread());
}

ImportFromDeviceDialog::ImportFromDeviceDialog(QWidget* parent):
    QmlDialogWrapper(
        appContext()->qmlEngine(),
        QUrl("Nx/Dialogs/ImportFromDevice/ImportFromDeviceDialog.qml"),
        {},
        parent),
    QnSessionAwareDelegate(parent),
    d(new Private(this))
{
    QmlProperty<QObject*>(rootObjectHolder(), "model") = &d->filterModel;

    d->filter.connectNotifySignal(this, [this]{ updateFilter(); });

    QObject::connect(rootObjectHolder()->object(), SIGNAL(doubleClicked(int)),
        this, SLOT(handleDoubleClick(int)));
}

ImportFromDeviceDialog::~ImportFromDeviceDialog()
{
}

bool ImportFromDeviceDialog::tryClose(bool /*force*/)
{
    reject();
    return true;
}

void ImportFromDeviceDialog::handleDoubleClick(int row)
{
    const auto camera = d->filterModel.index(row, 0).data(nx::vms::client::core::ResourceRole)
        .value<QnResourcePtr>();

    if (camera)
        menu()->trigger(menu::OpenInCurrentLayoutAction, menu::Parameters(camera));
}

void ImportFromDeviceDialog::updateFilter()
{
    d->filterModel.setFilterWildcard(d->filter);
}

} // namespace nx::vms::client::desktop::integrations
