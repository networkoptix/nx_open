// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "tier_usage_model.h"

#include <QtCore/QTimer>
#include <QtGui/QStandardItem>

#include <core/resource/layout_resource.h>
#include <nx/utils/log/assert.h>
#include <nx/vms/client/core/resource/session_resources_signal_listener.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/saas/tier_usage_common.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/saas/saas_service_manager.h>

namespace {

using namespace std::chrono;
static constexpr auto kResourcesSignalsAccumulationTimerInterval = 500ms;

} // namespace

namespace nx::vms::client::desktop::saas {

using namespace nx::vms::common::saas;

//-------------------------------------------------------------------------------------------------
// TierUsageModel::Private declaration.

struct TierUsageModel::Private
{
    Private(TierUsageModel* owner, ServiceManager* saasServiceManager);

    TierUsageModel* const q;
    ServiceManager* const serviceManager;

    std::unique_ptr<core::SessionResourcesSignalListener<QnLayoutResource>> resourcesListener;
    std::unique_ptr<QTimer> timer;

    // Cached data obtained from serviceManager.
    api::TierOveruseMap tierOveruseData;
    void updateTierOveruseData();

    QList<QStandardItem*> createTierLimitRow(
        nx::vms::api::SaasTierLimitName tierType,
        const nx::vms::api::TierOveruseData& overuseData);
};

//-------------------------------------------------------------------------------------------------
// TierUsageModel::Private definition.

TierUsageModel::Private::Private(
    TierUsageModel* owner,
    ServiceManager* saasServiceManager)
    :
    q(owner),
    serviceManager(saasServiceManager)
{
    timer = std::make_unique<QTimer>();
    timer->setInterval(kResourcesSignalsAccumulationTimerInterval);
}

void TierUsageModel::Private::updateTierOveruseData()
{
    auto newTierOveruseData = serviceManager->tierOveruseDetails();
    if (newTierOveruseData == tierOveruseData)
        return;

    tierOveruseData = newTierOveruseData;

    q->clear();
    for (auto it = tierOveruseData.begin(); it != tierOveruseData.end(); ++it)
        q->appendRow(createTierLimitRow(it->first, it->second));

    emit q->modelDataUpdated();
}

QList<QStandardItem*> TierUsageModel::Private::createTierLimitRow(
    nx::vms::api::SaasTierLimitName tierType,
    const nx::vms::api::TierOveruseData& overuseData)
{
    const auto hasDetails = !overuseData.details.empty();

    auto tierLimitTypeItem = new QStandardItem(tierLimitTypeToString(tierType));
    tierLimitTypeItem->setData(static_cast<int>(tierType), LimitTypeRole);

    auto tierLimitAllowedItem = new QStandardItem();
    auto tierLimitUsedItem = new QStandardItem();
    if (!isBooleanTierLimitType(tierType) && !hasDetails)
    {
        tierLimitAllowedItem->setData(QString::number(overuseData.allowed), Qt::DisplayRole);
        tierLimitUsedItem->setData(QString::number(overuseData.used), Qt::DisplayRole);
    }

    const auto resourcePool = serviceManager->systemContext()->resourcePool();
    for (auto it = overuseData.details.begin(); it != overuseData.details.end(); it++)
    {
        const auto resource = resourcePool->getResourceById(it->first);

        auto resourceNameItem = new QStandardItem(resource->getName());
        resourceNameItem->setData(qnResIconCache->icon(resource), Qt::DecorationRole);

        auto tierLimitAllowedItem = new QStandardItem(QString::number(overuseData.allowed));
        auto tierLimitUsedItem = new QStandardItem(QString::number(it->second));

        QList<QStandardItem*> resourceRow(
            {resourceNameItem, tierLimitAllowedItem, tierLimitUsedItem});

        tierLimitTypeItem->appendRow(resourceRow);
    }

    return {tierLimitTypeItem, tierLimitAllowedItem, tierLimitUsedItem};
}

//-------------------------------------------------------------------------------------------------
// TierUsageModel definition.


TierUsageModel::TierUsageModel(ServiceManager* serviceManager, QObject* parent):
    base_type(parent),
    d(new Private({this, serviceManager}))
{
    if (!NX_ASSERT(serviceManager))
        return;

    d->updateTierOveruseData();

    connect(d->timer.get(), &QTimer::timeout, this,
        [this]
        {
            d->updateTierOveruseData();
        });
}

TierUsageModel::~TierUsageModel()
{
}

Qt::ItemFlags TierUsageModel::flags(const QModelIndex&) const
{
    return Qt::ItemIsEnabled;
}

QVariant TierUsageModel::headerData(
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
            case LimitTypeColumn:
                return tr("Limitation");

            case AllowedLimitColumn:
                return tr("Allowed");

            case UsedLimitColumn:
                return tr("Current");

            default:
                return {};
        }
    }

    return base_type::headerData(section, orientation, role);
}

void TierUsageModel::setResourcesChangesTracking(SystemContext* systemContext, bool enabled)
{
    if (enabled)
        d->timer->start();
    else
        d->timer->stop();
}

bool TierUsageModel::isTrackingResourcesChanges() const
{
    return static_cast<bool>(d->resourcesListener);
}

} // namespace nx::vms::client::desktop::saas
