#include "resource_ptz_controller.h"

#include <core/ptz/client_ptz_controller_pool.h>
#include <core/resource_management/resource_pool.h>

namespace {

bool containsTrait(const QnPtzAuxilaryTraitList& list, Qn::PtzTrait trait)
{
    const auto it = std::find_if(list.begin(), list.end(),
        [trait](const QnPtzAuxilaryTrait& value)
        {
            return value.standardTrait() == trait;
        });
    return it != list.end();
}

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
            const auto controller = qnClientPtzPool->controller(resource);
            if (baseController() != controller)
                setBaseController(controller);
        });

    connect(this, &base_type::changed, this,
        [this](Qn::PtzDataFields fields)
        {
            if (fields.testFlag(Qn::CapabilitiesPtzField))
                emit capabilitiesChanged();
        });

    connect(this, &base_type::baseControllerChanged,
        this, &ResourcePtzController::availableChanged);
    connect(this, &base_type::baseControllerChanged,
        this, &ResourcePtzController::capabilitiesChanged);

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
    return baseController();
}

Ptz::Capabilities ResourcePtzController::capabilities() const
{
    QnPtzAuxilaryTraitList traits;

    if (!getAuxilaryTraits(&traits) || !containsTrait(traits, Qn::ManualAutoFocusPtzTrait))
        return Ptz::Capability::NoPtzCapabilities;

    NX_ASSERT(false);
    return Ptz::Capability::ManualAutoFocusCapability;
}

bool ResourcePtzController::setAutoFocus()
{
    return capabilities().testFlag(Ptz::Capability::ManualAutoFocusCapability)
        ? runAuxilaryCommand(Qn::ManualAutoFocusPtzTrait, QString())
        : false;
}
} // namespace mobile
} // namespace client
} // namespace nx

