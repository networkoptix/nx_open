#include "resource_ptz_controller.h"

#include <common/common_module.h>

#include <client_core/client_core_module.h>

#include <core/ptz/activity_ptz_controller.h>
#include <core/ptz/client_ptz_controller_pool.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/client/ptz/ptz_helpers.h>
#include <nx/client/mobile/ptz/ptz_availability_watcher.h>

namespace nx {
namespace client {
namespace mobile {

ResourcePtzController::ResourcePtzController(QObject* parent):
    m_availabilityWatcher(new PtzAvailabilityWatcher())
{
    connect(this, &ResourcePtzController::resourceIdChanged, this,
        [this]()
        {
            const auto resource = qnClientCoreModule->commonModule()
                ->resourcePool()->getResourceById(m_resourceId);
            auto controller = qnClientCoreModule->ptzControllerPool()->controller(resource);
            if (controller)
            {
                controller.reset(new QnActivityPtzController(qnClientCoreModule->commonModule(),
                    QnActivityPtzController::Client, controller));
            }
            setBaseController(controller);
        });

    connect(this, &base_type::changed, this,
        [this](Qn::PtzDataFields fields)
        {
            if (fields.testFlag(Qn::CapabilitiesPtzField))
                emit capabilitiesChanged();
            if (fields.testFlag(Qn::AuxiliaryTraitsPtzField))
                emit auxTraitsChanged();
            if (fields.testFlag(Qn::PresetsPtzField))
                emit presetsCountChanged();
            if (fields.testFlag(Qn::ActiveObjectPtzField) || fields.testFlag(Qn::PresetsPtzField))
                emit activePresetIndexChanged();
        });

    connect(this, &base_type::baseControllerChanged, this,
        [this]() { emit changed(Qn::AllPtzFields); });

    connect(m_availabilityWatcher, &PtzAvailabilityWatcher::availabilityChanged,
        this, &ResourcePtzController::availableChanged);
    connect(this, &ResourcePtzController::resourceIdChanged, this,
        [this](){ m_availabilityWatcher->setResourceId(QnUuid::fromStringSafe(resourceId()));});

    setParent(parent);
}

ResourcePtzController::~ResourcePtzController()
{
}

Ptz::Capabilities ResourcePtzController::operationalCapabilities() const
{
    return getCapabilities({nx::core::ptz::Type::operational});
}

QString ResourcePtzController::resourceId() const
{
    return m_resourceId.toString();
}

void ResourcePtzController::setResourceId(const QString& value)
{
    const auto id = QnUuid::fromStringSafe(value);
    if (id == m_resourceId)
        return;

    m_resourceId = id;
    emit resourceIdChanged();
}

bool ResourcePtzController::available() const
{
    return m_availabilityWatcher->available();
}

Ptz::Traits ResourcePtzController::auxTraits() const
{
    QnPtzAuxiliaryTraitList traits;
    if (!getAuxiliaryTraits(&traits, {nx::core::ptz::Type::operational}))
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
    if (!supports(Qn::GetActiveObjectPtzCommand))
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
            {nx::core::ptz::Type::operational});
}

} // namespace mobile
} // namespace client
} // namespace nx

