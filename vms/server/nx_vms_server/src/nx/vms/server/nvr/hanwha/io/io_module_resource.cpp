#include "io_module_resource.h"

#include <media_server/media_server_module.h>

#include <nx/vms/server/nvr/i_service.h>
#include <nx/vms/server/nvr/hanwha/common.h>

namespace nx::vms::server::nvr::hanwha {

static IIoManager* getIoManager(QnMediaServerModule* serverModule)
{
    nvr::IService* const nvrService = serverModule->nvrService();
    if (!NX_ASSERT(nvrService))
        return nullptr;

    nvr::IIoManager* ioManager = nvrService->ioManager();
    NX_ASSERT(ioManager);
    return ioManager;
}

IoModuleResource::IoModuleResource(QnMediaServerModule* serverModule):
    nx::vms::server::resource::Camera(serverModule)
{
    setCameraCapability(Qn::CameraCapability::ServerBoundCapability, true);
}

CameraDiagnostics::Result IoModuleResource::initializeCameraDriver()
{
    setFlags(flags() | Qn::io_module);
    setProperty(ResourcePropertyKey::kIoConfigCapability, QString("1"));
    setForcedLicenseType(Qn::LicenseType::LC_Free);

    const nvr::IIoManager* const ioManager = getIoManager(serverModule());
    if (!ioManager)
        return CameraDiagnostics::IOErrorResult("Unable to access NVR IO manager");

    setIoPortDescriptions(ioManager->portDesriptiors(), /*needMerge*/ false);
    saveProperties();

    return CameraDiagnostics::NoErrorResult();
}

QString IoModuleResource::getDriverName() const
{
    return kHanwhaPoeNvrDriverName;
}

QnIOStateDataList IoModuleResource::ioPortStates() const
{
    const nvr::IIoManager* const ioManager = getIoManager(serverModule());
    if (ioManager)
        return ioManager->portStates();

    return {};
}

bool IoModuleResource::setOutputPortState(
    const QString& outputId, bool isActive, unsigned int autoResetTimeoutMs)
{
    nvr::IIoManager* const ioManager = getIoManager(serverModule());
    if (!ioManager)
        return false;

    QString realOutputId = outputId;
    if (realOutputId.isEmpty()) //< Port should be chosen automatically.
    {
        const std::optional<QnIOPortData> descriptor =
            portDescriptor(/*portId*/ QString(), Qn::IOPortType::PT_Output);

        if (!NX_ASSERT(descriptor))
            return false;

        realOutputId = descriptor->id;
    }

    return ioManager->setOutputPortState(
        realOutputId,
        isActive ? IoPortState::active : IoPortState::inactive,
        std::chrono::milliseconds(autoResetTimeoutMs));
}

bool IoModuleResource::setIoPortDescriptions(QnIOPortDataList portDescriptors, bool needMerge)
{
    {
        QnMutexLocker lock(&m_mutex);
        m_portDescriptorsById.clear();
        for (const QnIOPortData& descriptor: portDescriptors)
            m_portDescriptorsById.emplace(descriptor.id, descriptor);
    }

    return base_type::setIoPortDescriptions(portDescriptors, needMerge);
}

void IoModuleResource::startInputPortStatesMonitoring()
{
    nvr::IIoManager* const ioManager = getIoManager(serverModule());
    if (!ioManager)
        return;

    m_handlerId = ioManager->registerStateChangeHandler(
        [this](const QnIOStateDataList& state) { handleStateChange(state); });
}

void IoModuleResource::stopInputPortStatesMonitoring()
{
    if (m_handlerId > 0)
        return;

    nvr::IIoManager* const ioManager = getIoManager(serverModule());
    if (!ioManager)
        return;

    ioManager->unregisterStateChangeHandler(m_handlerId);
}

std::optional<QnIOPortData> IoModuleResource::portDescriptor(
    const QString& portId,
    Qn::IOPortType portType) const
{
    QnMutexLocker lock(&m_mutex);
    if (portId.isEmpty() && portType != Qn::IOPortType::PT_Unknown)
    {
        for (const auto& [portId, descriptor]: m_portDescriptorsById)
        {
            if (descriptor.portType == portType)
                return descriptor;
        }
    }

    if (const auto it = m_portDescriptorsById.find(portId); it != m_portDescriptorsById.cend())
        return it->second;

    return std::nullopt;
}

void IoModuleResource::handleStateChange(const QnIOStateDataList& state)
{
    NX_DEBUG(this, "Handling IO port state change");
    for (const QnIOStateData& portState: state)
    {
        const auto descriptor = portDescriptor(portState.id);
        if (!NX_ASSERT(descriptor, "Unable to find descriptor for port '%1'", portState.id))
            continue;

        switch (descriptor->portType)
        {
            case Qn::IOPortType::PT_Input:
            {
                NX_DEBUG(this, "Emitting inputPortStateChanged signal for port %1, state %2",
                    portState.id, portState.isActive);

                emit inputPortStateChanged(
                    toSharedPointer(this),
                    portState.id,
                    portState.isActive,
                    portState.timestamp);
                break;
            }
            case Qn::IOPortType::PT_Output:
            {
                NX_DEBUG(this, "Emitting outputPortStateChanged signal for port %1, state %2",
                    portState.id, portState.isActive);
                emit outputPortStateChanged(
                    toSharedPointer(this),
                    portState.id,
                    portState.isActive,
                    portState.timestamp);
                break;
            }
            default:
                NX_ASSERT(false, "Only the 'input' and 'output' port types are supported");
        }
    }
}

} // namespace nx::vms::server::nvr::hanwha
