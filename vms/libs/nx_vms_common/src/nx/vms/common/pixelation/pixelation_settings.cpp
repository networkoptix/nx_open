// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "pixelation_settings.h"

#include <core/resource_access/resource_access_manager.h>
#include <core/resource_access/resource_access_subject.h>
#include <nx/analytics/taxonomy/abstract_state_watcher.h>
#include <nx/vms/api/data/pixelation_settings.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/common/system_settings.h>

namespace nx::vms::common {

struct PixelationSettings::Private:
    public QObject,
    public SystemContextAware
{
    QSet<QString> objectTypeIds;
    nx::vms::api::PixelationSettings settings;

    Private(SystemContext* context);
    void updateObjectTypes();
    void addObjectTypes(analytics::taxonomy::AbstractObjectType* objectType);
};

PixelationSettings::Private::Private(SystemContext* context):
    SystemContextAware(context)
{
}

void PixelationSettings::Private::updateObjectTypes()
{
    if (settings.isAllObjectTypes)
    {
        objectTypeIds.clear();
        return;
    }

    std::shared_ptr<analytics::taxonomy::AbstractState> state =
        systemContext()->analyticsTaxonomyState();

    objectTypeIds.clear();

    for (const QString& objectTypeId: settings.objectTypeIds)
    {
        analytics::taxonomy::AbstractObjectType* objectType = state->objectTypeById(objectTypeId);
        if (!NX_ASSERT(objectType))
            continue;

        addObjectTypes(objectType);
    }
}

void PixelationSettings::Private::addObjectTypes(
    analytics::taxonomy::AbstractObjectType* objectType)
{
    objectTypeIds.insert(objectType->id());
    for (const auto& derivedTypeId: objectType->derivedTypes())
        addObjectTypes(derivedTypeId);
}

PixelationSettings::PixelationSettings(SystemContext* context, QObject* parent):
    QObject(parent),
    d(new Private(context))
{
    d->updateObjectTypes();

    connect(context->globalSettings(), &common::SystemSettings::pixelationSettingsChanged,
        d.get(),
        [this, context]
        {
            d->settings = context->globalSettings()->pixelationSettings();
            d->updateObjectTypes();
            emit settingsChanged();
        });

    connect(
        context->analyticsTaxonomyStateWatcher(),
        &nx::analytics::taxonomy::AbstractStateWatcher::stateChanged,
        d.get(),
        [this]
        {
            d->updateObjectTypes();
            emit settingsChanged();
        });
}

PixelationSettings::~PixelationSettings()
{
}

bool PixelationSettings::isPixelationRequired(
    const QnUserResourcePtr& user,
    const nx::Uuid& cameraId,
    const QString& objectTypeId) const
{
    const bool canViewUnredacted =
        d->resourceAccessManager()->hasGlobalPermission(
            user, GlobalPermission::viewUnredactedVideo);

    const bool pixelateObject = d->settings.isAllObjectTypes
        || objectTypeId.isNull()
        || d->objectTypeIds.contains(objectTypeId);

    return !canViewUnredacted
        && !d->settings.excludeCameraIds.contains(cameraId)
        && pixelateObject;
}

bool PixelationSettings::isAllTypes() const
{
    return d->settings.isAllObjectTypes;
}

QSet<QString> PixelationSettings::objectTypeIds() const
{
    return d->objectTypeIds;
}

double PixelationSettings::intensity() const
{
    return d->settings.intensity;
}

} // namespace nx::vms::common
