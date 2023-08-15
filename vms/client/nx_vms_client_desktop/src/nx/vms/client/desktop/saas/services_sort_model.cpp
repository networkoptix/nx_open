// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "services_sort_model.h"

#include <QtCore/QCollator>

#include <nx/vms/api/data/saas_data.h>
#include <nx/vms/client/desktop/saas/services_usage_model.h>

namespace {

int serviceTypeOrder(const QString& serviceType)
{
    if (serviceType == nx::vms::api::SaasService::kLocalRecordingServiceType)
        return 0;

    if (serviceType == nx::vms::api::SaasService::kAnalyticsIntegrationServiceType)
        return 1;

    if (serviceType == nx::vms::api::SaasService::kCloudRecordingType)
        return 2;

    return 3;
}

} // namespace

namespace nx::vms::client::desktop::saas {

ServicesSortModel::ServicesSortModel(QObject* parent):
    base_type(parent)
{
}

bool ServicesSortModel::lessThan(const QModelIndex& lhs, const QModelIndex& rhs) const
{
    const auto lhsServiceType = lhs.data(ServicesUsageModel::ServiceTypeRole).toString();
    const auto rhsServiceType = rhs.data(ServicesUsageModel::ServiceTypeRole).toString();

    const auto lhsServiceTypeOrder = serviceTypeOrder(lhsServiceType);
    const auto rhsServiceTypeOrder = serviceTypeOrder(rhsServiceType);

    if (lhsServiceTypeOrder == rhsServiceTypeOrder)
    {
        QCollator collator;
        collator.setCaseSensitivity(Qt::CaseInsensitive);
        collator.setNumericMode(true);
        return collator.compare(
            lhs.data(Qt::DisplayRole).toString(), rhs.data(Qt::DisplayRole).toString()) < 0;
    }
    else
    {
        return lhsServiceTypeOrder < rhsServiceTypeOrder;
    }
}

} // namespace nx::vms::client::desktop::saas
