// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "services_usage_model.h"

#include <chrono>

#include <QtCore/QTimer>

#include <core/resource/camera_resource.h>
#include <core/resource/storage_resource.h>
#include <nx/utils/log/assert.h>
#include <nx/vms/client/core/resource/session_resources_signal_listener.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/html/html.h>
#include <nx/vms/common/saas/saas_service_manager.h>
#include <nx/vms/common/saas/saas_service_type_display_helper.h>
#include <nx/vms/license/saas/saas_service_usage_helper.h>
#include <nx/vms/time/formatter.h>

namespace {

using namespace std::chrono;
static constexpr auto kCameraSignalsAccumulationTimerInterval = 250ms;

} // namespace

namespace nx::vms::client::desktop::saas {

using namespace nx::vms::common::saas;
using namespace nx::vms::license::saas;

//-------------------------------------------------------------------------------------------------
// ServicesUsageModel::Private declaration.

struct ServicesUsageModel::Private
{
    Private(ServicesUsageModel* owner, ServiceManager* saasServiceManager);

    ServicesUsageModel* const q;
    ServiceManager* const serviceManager;

    std::unique_ptr<core::SessionResourcesSignalListener<QnVirtualCameraResource>> camerasListener;
    std::unique_ptr<QTimer> camerasSignalsAccumulationTimer;
    void startCamerasSignalsAccumulationTimer();

    // Cached data obtained from serviceManager.
    nx::vms::api::SaasData saasData;
    std::map<nx::Uuid, nx::vms::api::SaasService> servicesInfo;
    void updateUsedServicesData();

    // Ids of displayed services.
    std::vector<nx::Uuid> purchasedServicesIds() const;

    // Displayed values for the given service.
    QString serviceName(const nx::Uuid& serviceId) const;
    QString serviceType(const nx::Uuid& serviceId) const; //< As in license server replies.
    QString serviceTypeDisplay(const nx::Uuid& serviceId) const;
    int totalServiceQuantity(const nx::Uuid& serviceId) const;
    int usedServiceQuantity(const nx::Uuid& serviceId) const;
    bool hasCloudBackupStorage() const;
};

//-------------------------------------------------------------------------------------------------
// ServicesUsageModel::Private definition.

ServicesUsageModel::Private::Private(
    ServicesUsageModel* owner,
    ServiceManager* saasServiceManager)
    :
    q(owner),
    serviceManager(saasServiceManager)
{
    camerasSignalsAccumulationTimer = std::make_unique<QTimer>();
    camerasSignalsAccumulationTimer->setSingleShot(true);
    camerasSignalsAccumulationTimer->setInterval(kCameraSignalsAccumulationTimerInterval);
}

void ServicesUsageModel::Private::startCamerasSignalsAccumulationTimer()
{
    if (camerasSignalsAccumulationTimer->isActive())
        return;

    camerasSignalsAccumulationTimer->start();
}

void ServicesUsageModel::Private::updateUsedServicesData()
{
    q->beginResetModel();
    servicesInfo = serviceManager->services();
    saasData = serviceManager->data();

    // Cloud data can be inconsistent for some reasons, that's why we should filter out all
    // records for which we do not have correct service description.
    for (auto it = saasData.services.begin(); it != saasData.services.end(); )
    {
        const nx::Uuid& serviceId = it->first;
        if (servicesInfo.contains(serviceId))
        {
            ++it;
        }
        else
        {
            NX_WARNING(this,
                "Inconsistent SAAS data: service id %1 is not listed in services", serviceId);
            it = saasData.services.erase(it);
        }
    }

    q->endResetModel();
}

std::vector<nx::Uuid> ServicesUsageModel::Private::purchasedServicesIds() const
{
    std::vector<nx::Uuid> servicesIds;

    for (const auto [serviceId, _]: saasData.services)
        servicesIds.push_back(serviceId);

    return servicesIds;
}

QString ServicesUsageModel::Private::serviceName(const nx::Uuid& serviceId) const
{
    if (!NX_ASSERT(servicesInfo.contains(serviceId)))
        return {};

    return servicesInfo.at(serviceId).displayName;
}

QString ServicesUsageModel::Private::serviceType(const nx::Uuid& serviceId) const
{
    if (!NX_ASSERT(servicesInfo.contains(serviceId)))
        return {};

    return servicesInfo.at(serviceId).type;
}

QString ServicesUsageModel::Private::serviceTypeDisplay(const nx::Uuid& serviceId) const
{
    if (!NX_ASSERT(servicesInfo.contains(serviceId)))
        return {};

    return ServiceTypeDisplayHelper::serviceTypeDisplayString(servicesInfo.at(serviceId).type);
}

int ServicesUsageModel::Private::totalServiceQuantity(const nx::Uuid& serviceId) const
{
    if (!NX_ASSERT(saasData.services.contains(serviceId)))
        return {};

    return saasData.services.at(serviceId).quantity;
}

bool ServicesUsageModel::Private::hasCloudBackupStorage() const
{
    const auto systemContext = serviceManager->systemContext();
    const auto storages  = systemContext->resourcePool()->storages();
    return std::any_of(storages.begin(), storages.end(),
        [](const auto& storage)
        {
            return storage->isBackup() && storage->isCloudStorage();
        });
}

int ServicesUsageModel::Private::usedServiceQuantity(const nx::Uuid& serviceId) const
{
    if (!NX_ASSERT(servicesInfo.contains(serviceId)))
        return 0;

    const auto service = servicesInfo.at(serviceId);
    const auto systemContext = serviceManager->systemContext();

    if (service.type == nx::vms::api::SaasService::kLocalRecordingServiceType)
    {
        LocalRecordingUsageHelper usageHelper(systemContext);
        return usageHelper.cameraGroupsByService().contains(serviceId)
            ? usageHelper.cameraGroupsByService().at(serviceId).size()
            : 0;
    }
    else if (service.type == nx::vms::api::SaasService::kLiveViewServiceType)
    {
        LiveViewUsageHelper usageHelper(systemContext);
        return usageHelper.info().inUse;
    }
    else if (service.type == nx::vms::api::SaasService::kCloudRecordingType)
    {
        if (!hasCloudBackupStorage())
            return 0;
        CloudStorageServiceUsageHelper usageHelper(systemContext);
        const auto allInfoByService = usageHelper.allInfoByService();
        return allInfoByService.contains(serviceId)
            ? allInfoByService.at(serviceId).inUse
            : 0;
    }
    else if (service.type == nx::vms::api::SaasService::kAnalyticsIntegrationServiceType)
    {
        IntegrationServiceUsageHelper usageHelper(systemContext);
        return usageHelper.camerasByService().contains(serviceId)
            ? usageHelper.camerasByService().at(serviceId).size()
            : 0;
    }
    else
    {
        NX_ASSERT(false, "Unexpected service type");
        return 0;
    }
}

//-------------------------------------------------------------------------------------------------
// ServicesUsageModel definition.

ServicesUsageModel::ServicesUsageModel(ServiceManager* serviceManager, QObject* parent):
    base_type(parent),
    d(new Private({this, serviceManager}))
{
    if (!NX_ASSERT(serviceManager))
        return;

    connect(d->serviceManager, &ServiceManager::dataChanged, this,
        [this] { d->updateUsedServicesData(); });
    d->updateUsedServicesData();

    connect(d->camerasSignalsAccumulationTimer.get(), &QTimer::timeout, this,
        [this]
        {
            if (!rowCount())
                return;

            const auto topLeft = index(0, 0);
            const auto bottomRight = index(rowCount() - 1, columnCount() - 1);
            emit dataChanged(topLeft, bottomRight);
        });
}

ServicesUsageModel::~ServicesUsageModel()
{
}

int ServicesUsageModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : Column::ColumnCount;
}

QVariant ServicesUsageModel::data(const QModelIndex& index, int role) const
{
    const auto serviceId = d->purchasedServicesIds().at(index.row());
    const auto serviceType = d->serviceType(serviceId);
    nx::vms::api::ServiceTypeStatus serviceTypeStatus{};
    auto it = d->saasData.security.status.find(serviceType);
    if (it != d->saasData.security.status.end())
        serviceTypeStatus = it->second;

    // Service overuse status is deduced from the actual number of services used rather than taken
    // from SaasSecurity structure which is updated in a long intervals. It's done this way to make
    // overused status responsive to user actions.
    const auto serviceOverused =
        d->usedServiceQuantity(serviceId) > d->totalServiceQuantity(serviceId);

    if (role == Qt::DisplayRole)
    {
        switch (index.column())
        {
            case ServiceNameColumn:
                return d->serviceName(serviceId);

            case ServiceTypeColumn:
                return d->serviceTypeDisplay(serviceId);

            case TotalQantityColumn:
                return d->totalServiceQuantity(serviceId);

            case UsedQantityColumn:
                return d->usedServiceQuantity(serviceId);

            default:
                return {};
        }
    }

    if (role == Qt::ToolTipRole
        && index.column() == ServiceOveruseWarningIconColumn
        && serviceOverused)
    {
        using namespace nx::vms::common;

        QStringList toolTipLines;
        toolTipLines.push_back(html::paragraph(
            tr("The number of devices using this service exceeds the available capacity. "
               "Add more services or disable the services on some devices.")));

        if (serviceTypeStatus.status == nx::vms::api::UseStatus::overUse)
        {
            const auto formatter = nx::vms::time::Formatter::system();
            const auto expirationDateString =
                formatter->toString(serviceTypeStatus.issueExpirationDate);

            toolTipLines.push_back(html::paragraph(
                tr("On %1, the system will automatically disable the service on some devices.",
                    "%1 will be substituted with date and time").arg(expirationDateString)));
        }

        return toolTipLines.join("");
    }

    if (role == Qt::DecorationRole
        && index.column() == ServiceOveruseWarningIconColumn
        && serviceOverused)
    {
        return qnSkin->icon(core::kAlertIcon);
    }

    if (role == ServiceTypeRole)
        return serviceType;

    if (role == ServiceOverusedRole)
        return serviceOverused;

    return {};
}

QVariant ServicesUsageModel::headerData(
    int section,
    Qt::Orientation orientation,
    int role) const
{
    if (orientation == Qt::Vertical)
        return base_type::headerData(section, orientation, role);

    if (role == Qt::TextAlignmentRole)
        return Qt::AlignLeft;

    if (role == Qt::DisplayRole)
    {
        switch (section)
        {
            case ServiceNameColumn:
                return tr("Name");

            case ServiceTypeColumn:
                return tr("Type");

            case TotalQantityColumn:
                return tr("Total");

            case UsedQantityColumn:
                return tr("Used");

            default:
                return {};
        }
    }

    return base_type::headerData(section, orientation, role);
}

QModelIndex ServicesUsageModel::index(int row, int column, const QModelIndex&) const
{
    return createIndex(row, column);
}

QModelIndex ServicesUsageModel::parent(const QModelIndex&) const
{
    return {};
}

int ServicesUsageModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : d->purchasedServicesIds().size();
}

void ServicesUsageModel::setCamerasChangesTracking(SystemContext* systemContext, bool enabled)
{
    if (isTrackingCamerasChanges() == enabled)
        return;

    if (enabled)
    {
        d->camerasListener
            = std::make_unique<core::SessionResourcesSignalListener<QnVirtualCameraResource>>(
                systemContext);

        d->camerasListener->addOnSignalHandler(
            &QnVirtualCameraResource::statusChanged,
            [this](const auto&) { d->startCamerasSignalsAccumulationTimer(); });

        d->camerasListener->addOnSignalHandler(
            &QnVirtualCameraResource::backupPolicyChanged,
            [this](const auto&) { d->startCamerasSignalsAccumulationTimer(); });

        d->camerasListener->addOnSignalHandler(
            &QnVirtualCameraResource::userEnabledAnalyticsEnginesChanged,
            [this](const auto&) { d->startCamerasSignalsAccumulationTimer(); });

        d->camerasListener->start();
    }
    else
    {
        d->camerasListener.reset();
    }
}

bool ServicesUsageModel::isTrackingCamerasChanges() const
{
    return static_cast<bool>(d->camerasListener);
}

} // namespace nx::vms::client::desktop::saas
