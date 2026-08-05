// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "services_usage_model.h"

#include <QtCore/QStringList>

#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/common/html/html.h>
#include <nx/vms/common/saas/saas_service_manager.h>
#include <nx/vms/common/saas/saas_service_type_display_helper.h>
#include <nx/vms/time/formatter.h>

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
    const auto it = servicesInfo.find(serviceId);
    if (!NX_ASSERT(it != servicesInfo.end()))
        return {};

    return it->second.displayName;
}

QString ServicesUsageModel::Private::serviceType(const nx::Uuid& serviceId) const
{
    const auto it = servicesInfo.find(serviceId);
    if (!NX_ASSERT(it != servicesInfo.end()))
        return {};

    return it->second.type;
}

QString ServicesUsageModel::Private::serviceTypeDisplay(const nx::Uuid& serviceId) const
{
    const auto it = servicesInfo.find(serviceId);
    if (!NX_ASSERT(it != servicesInfo.end()))
        return {};

    return ServiceTypeDisplayHelper::serviceTypeDisplayString(it->second.type);
}

int ServicesUsageModel::Private::totalServiceQuantity(const nx::Uuid& serviceId) const
{
    const auto it = saasData.services.find(serviceId);
    if (!NX_ASSERT(it != saasData.services.end()))
        return {};

    return it->second.quantity;
}

int ServicesUsageModel::Private::usedServiceQuantity(const nx::Uuid& serviceId) const
{
    const auto it = saasData.services.find(serviceId);
    if (!NX_ASSERT(it != saasData.services.end()))
        return {};

    // The usage is taken from the Channel Partner Server data as is. It is calculated there from
    // the usage reports sent by the Servers, thus it also accounts for the services whose usage is
    // reported by plugins (e.g. Nx Maps) and cannot be deduced from the resource pool data.
    return it->second.used;
}

//-------------------------------------------------------------------------------------------------
// ServicesUsageModel definition.

ServicesUsageModel::ServicesUsageModel(ServiceManager* serviceManager, QObject* parent):
    base_type(parent),
    d(new Private{this, serviceManager})
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
    const auto serviceId = d->purchasedServicesIds().at(index.row());
    const auto serviceType = d->serviceType(serviceId);
    nx::vms::api::ServiceTypeStatus serviceTypeStatus{};
    auto it = d->saasData.security.status.find(serviceType);
    if (it != d->saasData.security.status.end())
        serviceTypeStatus = it->second;

    // SaasSecurity::status reports overuse detected by the license server per service type, so it
    // cannot tell which of the services of that type is overused. That's why the row overuse
    // status is deduced from the same used and total values as displayed.
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

} // namespace nx::vms::client::desktop::saas
