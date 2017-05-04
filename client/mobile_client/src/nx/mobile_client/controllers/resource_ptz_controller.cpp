#include "resource_ptz_controller.h"

#include <core/ptz/activity_ptz_controller.h>
#include <core/ptz/client_ptz_controller_pool.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>

namespace {

#if defined(NO_MOBILE_PTZ_SUPPORT)
    static constexpr bool kSupportMobilePtz = false;
#else
    static constexpr bool kSupportMobilePtz = true;
#endif

}

namespace nx {
namespace client {
namespace mobile {

ResourcePtzController::ResourcePtzController(QObject* parent):
    base_type()
{
    connect(this, &ResourcePtzController::uniqueResourceIdChanged, this,
        [this]()
        {
            const auto resource = qnResPool->getResourceById(m_uniqueResourceId);
            auto controller = qnClientPtzPool->controller(resource);
            if (controller)
            {
                controller.reset(new QnActivityPtzController(
                    QnActivityPtzController::Client, controller));
            }
            setBaseController(controller);
        });

    connect(this, &base_type::changed, this,
        [this](Qn::PtzDataFields fields)
        {
            if (fields.testFlag(Qn::CapabilitiesPtzField))
                emit capabilitiesChanged();
            if (fields.testFlag(Qn::AuxilaryTraitsPtzField))
                emit auxTraitsChanged();
            if (fields.testFlag(Qn::PresetsPtzField))
            {
                emit presetsCountChanged();
                emit activePresetIndexChanged();
            }
            if (fields.testFlag(Qn::ActiveObjectPtzField))
                emit activePresetIndexChanged();
        });

    connect(this, &base_type::baseControllerChanged, this,
        [this]() { emit changed(Qn::AllPtzFields); });

    connect(this, &base_type::baseControllerChanged,
        this, &ResourcePtzController::availableChanged);

    setParent(parent);
}

QUuid ResourcePtzController::uniqueResourceId() const
{
    return m_uniqueResourceId.toString();
}

void ResourcePtzController::setUniqueResourceId(const QUuid& value)
{
    if (value == m_uniqueResourceId)
        return;

    m_uniqueResourceId = value;
    emit uniqueResourceIdChanged();
}

bool ResourcePtzController::available() const
{
    const auto ptzResource = baseController() ? baseController()->resource() : QnResourcePtr();
    if (!ptzResource)
        return false;

    const auto server = ptzResource->getParentResource().dynamicCast<QnMediaServerResource>();
    if (!server || server->getVersion() < QnSoftwareVersion(2, 6))
        return false;

    return kSupportMobilePtz && baseController();
}

Ptz::Traits ResourcePtzController::auxTraits() const
{
    QnPtzAuxilaryTraitList traits;
    if (!getAuxilaryTraits(&traits))
        return Ptz::NoPtzTraits;

    Ptz::Traits result = Ptz::NoPtzTraits;
    for (const auto& trait: traits)
        result |= trait.standardTrait();

    return result;
}

int ResourcePtzController::presetsCount() const
{
    if (!getCapabilities().testFlag(Ptz::PresetsPtzCapability))
        return 0;

    QnPtzPresetList presets;
    return getPresets(&presets) ? presets.size() : 0;
}

int ResourcePtzController::activePresetIndex() const
{
    if (!supports(Qn::GetActiveObjectPtzCommand))
        return -1;

    QnPtzObject activeObject;
    if (!getActiveObject(&activeObject) || activeObject.type != Qn::PresetPtzObject)
        return -1;

    QnPtzPresetList presets;
    if (!getPresets(&presets))
        return -1;

    for (int i = 0; i != presets.count(); ++i)
    {
        if (presets.at(i).id == activeObject.id)
            return i;
    }
    return -1;
}

bool ResourcePtzController::setPresetByIndex(int index)
{
    if (!getCapabilities().testFlag(Ptz::PresetsPtzCapability)
        || !qBetween(0, index, presetsCount()))
    {
        return false;
    }

    QnPtzPresetList presets;
    return getPresets(&presets) && activatePreset(presets.at(index).id,
        QnAbstractPtzController::MaxPtzSpeed);
}

bool ResourcePtzController::setPresetById(const QString& id)
{
    if (!getCapabilities().testFlag(Ptz::PresetsPtzCapability))
        return false;

    QnPtzPresetList presets;
    if (!getPresets(&presets))
        return false;

    const bool found = std::find_if(presets.begin(), presets.end(),
        [id](const QnPtzPreset& preset) { return id == preset.id; }) != presets.end();

    return found && activatePreset(id, QnAbstractPtzController::MaxPtzSpeed);

}

bool ResourcePtzController::setAutoFocus()
{
    return auxTraits().testFlag(Ptz::ManualAutoFocusPtzTrait)
        && runAuxilaryCommand(Ptz::ManualAutoFocusPtzTrait, QString());
}

} // namespace mobile
} // namespace client
} // namespace nx

