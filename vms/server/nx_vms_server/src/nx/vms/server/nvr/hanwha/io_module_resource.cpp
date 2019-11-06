#include "io_module_resource.h"

#include <nx/vms/server/nvr/hanwha/common.h>

#include <nx/fusion/serialization/lexical.h>
#include <media_server/media_server_module.h>
#include <nx/vms/server/nvr/i_service.h>

namespace nx::vms::server::nvr::hanwha {

static IIoController* getIoController(QnMediaServerModule* serverModule)
{
    nvr::IService* const nvrService = serverModule->nvrService();
    if (!NX_ASSERT(nvrService))
        return nullptr;

    nvr::IIoController* ioController = nvrService->ioController();
    NX_ASSERT(ioController);
    return ioController;
}

static bool verifyPortState(
    const QnIOStateData& desiredPortState,
    const QnIOStateDataList& currentPortStates)
{
    for (const auto& portState: currentPortStates)
    {
        if (desiredPortState.id == portState.id)
            return desiredPortState.isActive == portState.isActive;
    }

    return false;
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
    setProperty(
        ResourcePropertyKey::kForcedLicenseType,
        QnLexical::serialized(Qn::LicenseType::LC_Free));

    const nvr::IIoController* const ioController = getIoController(serverModule());
    if (ioController)
        setIoPortDescriptions(ioController->portDesriptiors(), /*needMerge*/ false);

    saveProperties();

    return CameraDiagnostics::NoErrorResult();
}

QString IoModuleResource::getDriverName() const
{
    return kHanwhaPoeNvrDriverName;
}

QnIOStateDataList IoModuleResource::ioPortStates() const
{
    const nvr::IIoController* const ioController = getIoController(serverModule());
    if (ioController)
        return ioController->portStates();

    return {};
}

bool IoModuleResource::setOutputPortState(
    const QString& outputId, bool isActive, unsigned int autoResetTimeoutMs)
{
    nvr::IIoController* const ioController = getIoController(serverModule());
    if (!ioController)
        return false;

    QnIOStateData state;
    state.id = outputId;
    state.isActive = isActive;

    const QnIOStateDataList updatedStates =
        ioController->setOutputPortStates({state}, std::chrono::milliseconds(autoResetTimeoutMs));

    return verifyPortState(state, updatedStates);
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
    nvr::IIoController* const ioController = getIoController(serverModule());
    if (!ioController)
        return;

    m_handlerId = ioController->registerStateChangeHandler(
        [this](const QnIOStateDataList& state) { handleStateChange(state); });
}

void IoModuleResource::stopInputPortStatesMonitoring()
{
    if (m_handlerId.isNull())
        return;

    nvr::IIoController* const ioController = getIoController(serverModule());
    if (!ioController)
        return;

    ioController->unregisterStateChangeHandler(m_handlerId);
}

std::optional<QnIOPortData> IoModuleResource::portDescriptor(const QString& portId) const
{
    QnMutexLocker lock(&m_mutex);
    if (const auto it = m_portDescriptorsById.find(portId); it != m_portDescriptorsById.cend())
        return it->second;

    return std::nullopt;
}

void IoModuleResource::handleStateChange(const QnIOStateDataList& state)
{
    for (const QnIOStateData& portState: state)
    {
        const auto descriptor = portDescriptor(portState.id);
        if (!NX_ASSERT(descriptor))
            continue;

        switch (descriptor->portType)
        {
            case Qn::IOPortType::PT_Input:
            {
                emit inputPortStateChanged(
                    toSharedPointer(this),
                    portState.id,
                    portState.isActive,
                    portState.timestamp);
                break;
            }
            case Qn::IOPortType::PT_Output:
            {
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
