// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "services_usage_model.h"

#include <nx/utils/log/assert.h>
#include <nx/vms/common/saas/saas_service_manager.h>
#include <nx/vms/common/saas/saas_service_type_display_helper.h>
#include <nx/vms/common/saas/saas_service_usage_helper.h>

namespace nx::vms::client::desktop::saas {

using namespace nx::vms::common::saas;

//-------------------------------------------------------------------------------------------------
// ServicesUsageModel::Private declaration.

struct ServicesUsageModel::Private
{
    ServicesUsageModel* const q;
    ServiceManager* const serviceManager;

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
};

//-------------------------------------------------------------------------------------------------
// ServicesUsageModel::Private definition.

void ServicesUsageModel::Private::updateUsedServicesData()
{
    q->beginResetModel();
    saasData = serviceManager->data();
    servicesInfo = serviceManager->services();
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

int ServicesUsageModel::Private::usedServiceQuantity(const nx::Uuid& serviceId) const
{
    // TODO: #vbreus Track changes in used services quantities.

    if (!NX_ASSERT(servicesInfo.contains(serviceId)))
        return 0;

    const auto service = servicesInfo.at(serviceId);
    const auto systemContext = serviceManager->systemContext();

    if (service.type == nx::vms::api::SaasService::kLocalRecordingServiceType)
    {
        LocalRecordingUsageHelper usageHelper(systemContext);
        return usageHelper.camerasByService().contains(serviceId)
            ? usageHelper.camerasByService().at(serviceId).size()
            : 0;
    }
    else if (service.type == nx::vms::api::SaasService::kCloudRecordingType)
    {
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
    const auto rowServiceId = d->purchasedServicesIds().at(index.row());

    if (role == Qt::DisplayRole)
    {
        switch (index.column())
        {
            case ServiceNameColumn:
                return d->serviceName(rowServiceId);

            case ServiceTypeColumn:
                return d->serviceTypeDisplay(rowServiceId);

            case TotalQantityColumn:
                return d->totalServiceQuantity(rowServiceId);

            case UsedQantityColumn:
                return d->usedServiceQuantity(rowServiceId);

            default:
                NX_ASSERT("Unexpected column");
                return {};
        }
    }

    if (role == ServiceTypeRole)
        return d->serviceType(rowServiceId);

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
                NX_ASSERT("Unexpected column");
                return {};
        }
    }

    return base_type::headerData(section, orientation, role);
}

QModelIndex ServicesUsageModel::index(int row, int column, const QModelIndex&) const
{
    return createIndex(row, column);
}

int ServicesUsageModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : d->purchasedServicesIds().size();
}

QModelIndex ServicesUsageModel::parent(const QModelIndex&) const
{
    return {};
}

} // namespace nx::vms::client::desktop::saas
