// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "watermark_watcher.h"

#include <unordered_map>

#include <QtCore/QSize>
#include <QtQml/QtQml>

#include <core/resource/avi/avi_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_access/resource_access_subject.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/core/media/watermark_image_provider.h>
#include <nx/vms/client/core/system_context.h>
#include <nx/vms/client/core/watchers/user_watcher.h>
#include <nx/vms/common/system_settings.h>

namespace nx::vms::client::core {

namespace {

struct WatermarkData
{
    QSize size;

    bool operator==(const WatermarkData& /*other*/) const = default;
};

using WatermarkDataHash = std::unordered_map<QnUuid, WatermarkData>;

} // namespace

struct WatermarkWatcher::Private: public QObject
{
    WatermarkWatcher* const q;

    WatermarkDataHash data;
    nx::core::Watermark watermark;
    String user;

    Private(WatermarkWatcher* q);

    void updateUser();
    void updateWatermark();
};

WatermarkWatcher::Private::Private(WatermarkWatcher* q):
    q(q)
{
    const auto userWatcher = q->systemContext()->userWatcher();
    connect(userWatcher, &UserWatcher::userChanged, this, &Private::updateUser);
    updateUser();

    connect(q->systemSettings(),
        &common::SystemSettings::watermarkChanged,
        this,
        &Private::updateWatermark);
}

void WatermarkWatcher::Private::updateUser()
{
    const auto value = q->systemContext()->userWatcher()->userName();
    if (value == user)
        return;

    user = value;
    updateWatermark();
}

void WatermarkWatcher::Private::updateWatermark()
{
    const auto settings = q->systemSettings()->watermarkSettings();
    const auto value = nx::core::Watermark{settings, user};
    if (watermark == value)
        return;

    watermark = value;

    for (const auto& [id, _]: data)
        emit q->watermarkImageUrlChanged(id);
}

//-------------------------------------------------------------------------------------------------

void WatermarkWatcher::registerQmlType()
{
    qmlRegisterUncreatableType<WatermarkWatcher>("nx.client.mobile", 1, 0, "WatermarkWatcher",
        "System Context instance of Watermark Watcher should be used");
}

WatermarkWatcher::WatermarkWatcher(SystemContext* systemContext, QObject* parent):
    base_type(parent),
    SystemContextAware(systemContext),
    d(new Private(this))
{
}

WatermarkWatcher::~WatermarkWatcher()
{
}

void WatermarkWatcher::addWatermarkImageUrlWatcher(
    const QnUuid& id,
    const QSize& size)
{
    const auto it = d->data.find(id);

    WatermarkData data = WatermarkData{size};
    bool hasChanges = true;
    if (it == d->data.cend())
        d->data.insert(std::make_pair(id, data));
    else if (it->second == data)
        hasChanges = false;
    else
        it->second = data;

    if (hasChanges)
        emit watermarkImageUrlChanged(id);
}

void WatermarkWatcher::removeWatermarkImageUrlWatcher(const QnUuid& id)
{
    const auto it = d->data.find(id);
    if (it == d->data.cend())
        return;

    d->data.erase(it);
    emit watermarkImageUrlChanged(id);
}

QUrl WatermarkWatcher::watermarkImageUrl(const QnUuid& id) const
{
    if (!d->watermark.visible())
        return {};

    const auto it = d->data.find(id);
    if (it == d->data.cend() || it->second.size.isEmpty())
        return {};

    const auto user = systemContext()->userWatcher()->user();
    const auto hasPowerUserPermissions =
        systemContext()->resourceAccessManager()->hasPowerUserPermissions(user);
    if (hasPowerUserPermissions)
        return {};

    return WatermarkImageProvider::makeWatermarkSourceUrl(d->watermark, it->second.size);
}

void WatermarkWatcher::updateWatermarkImageUrlSize(
    const QnUuid& id,
    const QSize& size)
{
    const auto it = d->data.find(id);
    if (it == d->data.cend() || it->second.size == size)
        return;

    it->second.size = size;
    emit watermarkImageUrlChanged(id);
}

} // nx::vms::client::core
