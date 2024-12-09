// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_ptz_controller.h"

#include <core/ptz/activity_ptz_controller.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/client/mobile/ptz/ptz_availability_watcher.h>
#include <nx/vms/client/core/ptz/client_ptz_controller_pool.h>
#include <nx/vms/client/core/ptz/helpers.h>
#include <nx/vms/client/core/resource/camera.h>
#include <nx/vms/client/core/system_context.h>

namespace nx {
namespace client {
namespace mobile {

ResourcePtzController::ResourcePtzController(QObject* parent)
{
    connect(this, &base_type::changed, this,
        [this](DataFields fields)
        {
            if (fields.testFlag(DataField::capabilities))
                emit capabilitiesChanged();
            if (fields.testFlag(DataField::auxiliaryTraits))
                emit auxTraitsChanged();
            if (fields.testFlag(DataField::presets))
                emit presetsCountChanged();
            if (fields.testFlag(DataField::activeObject) || fields.testFlag(DataField::presets))
                emit activePresetIndexChanged();
        });

    connect(this, &base_type::baseControllerChanged, this,
        [this]() { emit changed(DataField::all); });

    setParent(parent);
}

ResourcePtzController::~ResourcePtzController()
{
}

Ptz::Capabilities ResourcePtzController::operationalCapabilities() const
{
    return getCapabilities({Type::operational});
}

QnResource* ResourcePtzController::rawResource() const
{
    return resource().data();
}

void ResourcePtzController::setRawResource(QnResource* value)
{
    auto resource = value ? value->toSharedPointer() : QnResourcePtr();

    QnPtzControllerPtr controller;
    if (auto systemContext = nx::vms::client::core::SystemContext::fromResource(resource))
        controller = systemContext->ptzControllerPool()->controller(resource);

    if (controller)
    {
        controller.reset(new QnActivityPtzController(
            QnActivityPtzController::Client,
            controller));
        controller->invalidate();
    }
    setBaseController(controller);

    m_availabilityWatcher.reset();
    if (auto camera = resource.dynamicCast<nx::vms::client::core::Camera>())
    {
        m_availabilityWatcher.reset(new PtzAvailabilityWatcher(camera));
        connect(m_availabilityWatcher.get(), &PtzAvailabilityWatcher::availabilityChanged, this,
            &ResourcePtzController::availableChanged);
    }

    emit resourceChanged();
    emit availableChanged();
}

bool ResourcePtzController::available() const
{
    return m_availabilityWatcher->available();
}

Ptz::Traits ResourcePtzController::auxTraits() const
{
    QnPtzAuxiliaryTraitList traits;
    if (!getAuxiliaryTraits(&traits, {Type::operational}))
        return Ptz::NoPtzTraits;

    Ptz::Traits result = Ptz::NoPtzTraits;
    for (const auto& trait: traits)
        result |= trait.standardTrait();

    return result;
}

int ResourcePtzController::presetsCount() const
{
    if (!operationalCapabilities().testFlag(Ptz::PresetsPtzCapability))
        return 0;

    QnPtzPresetList presets;
    return nx::vms::client::core::ptz::helpers::getSortedPresets(this, presets) ? presets.size() : 0;
}

int ResourcePtzController::activePresetIndex() const
{
    if (!supports(Command::getActiveObject))
        return -1;

    QnPtzObject activeObject;
    if (!getActiveObject(&activeObject)
        || activeObject.type != Qn::PresetPtzObject)
    {
        return -1;
    }

    QnPtzPresetList presets;
    if (!nx::vms::client::core::ptz::helpers::getSortedPresets(this, presets) || presets.isEmpty())
        return -1;

    for (int i = 0; i != presets.count(); ++i)
    {
        if (presets.at(i).id == activeObject.id)
            return i;
    }
    return -1;
}

int ResourcePtzController::capabilities() const
{
    return (int) operationalCapabilities();
}

bool ResourcePtzController::setPresetByIndex(int index)
{
    if (!operationalCapabilities().testFlag(Ptz::PresetsPtzCapability)
        || !qBetween(0, index, presetsCount()))
    {
        return false;
    }

    QnPtzPresetList presets;
    return nx::vms::client::core::ptz::helpers::getSortedPresets(this, presets)
        && !presets.isEmpty()
        && activatePreset(presets.at(index).id, QnAbstractPtzController::MaxPtzSpeed);
}

bool ResourcePtzController::setPresetById(const QString& id)
{
    if (!operationalCapabilities().testFlag(Ptz::PresetsPtzCapability))
        return false;

    QnPtzPresetList presets;
    if (!nx::vms::client::core::ptz::helpers::getSortedPresets(this, presets) || presets.isEmpty())
        return false;

    const bool found = std::find_if(presets.begin(), presets.end(),
        [id](const QnPtzPreset& preset) { return id == preset.id; }) != presets.end();

    return found
        && activatePreset(id, QnAbstractPtzController::MaxPtzSpeed);
}

int ResourcePtzController::indexOfPreset(const QString& id) const
{
    QnPtzPresetList presets;
    if (!nx::vms::client::core::ptz::helpers::getSortedPresets(this, presets) || presets.isEmpty())
        return -1;

    const auto it = std::find_if(presets.begin(), presets.end(),
        [id](const QnPtzPreset& preset) { return id == preset.id; });

    return it == presets.end() ? -1 : it - presets.begin();
}

bool ResourcePtzController::setAutoFocus()
{
    return auxTraits().testFlag(Ptz::ManualAutoFocusPtzTrait)
        && runAuxiliaryCommand(
            Ptz::ManualAutoFocusPtzTrait,
            QString(),
            {Type::operational});
}

bool ResourcePtzController::continuousMove(const QVector3D& speed)
{
    const auto speedVector = Vector(speed, Vector::kPtzComponents);
    return base_type::continuousMove(speedVector, Options());
}

} // namespace mobile
} // namespace client
} // namespace nx
