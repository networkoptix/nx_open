#include "io_module_resource.h"

#include <utils/common/synctime.h>

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
    return ioManager;
}

static std::map<QString, Qn::IODefaultState> toCircuitTypeByPort(
    const QnIOPortDataList& portDescriptions)
{
    std::map<QString, Qn::IODefaultState> circuitTypeByPort;
    for (const QnIOPortData& portDescription: portDescriptions)
    {
        circuitTypeByPort[portDescription.id] =
            portDescription.portType == Qn::IOPortType::PT_Input
                ? portDescription.iDefaultState
                : portDescription.oDefaultState;
    }

    return circuitTypeByPort;
}

IoModuleResource::IoModuleResource(QnMediaServerModule* serverModule):
    nx::vms::server::resource::Camera(serverModule)
{
    setCameraCapability(Qn::CameraCapability::ServerBoundCapability, true);
}

CameraDiagnostics::Result IoModuleResource::initializeCameraDriver()
{
    setFlags(flags() | Qn::io_module);
    setCameraCapability(Qn::CameraCapability::noAnalytics, true);
    setProperty(ResourcePropertyKey::kIoConfigCapability, QString("1"));
    setForcedLicenseType(Qn::LicenseType::LC_Free);

    nvr::IIoManager* const ioManager = getIoManager(serverModule());
    if (!ioManager)
        return CameraDiagnostics::IOErrorResult("Unable to access NVR IO manager");

    const QnIOPortDataList portDescriptions = mergedPortDescriptions();
    setIoPortDescriptions(portDescriptions, /*needMerge*/ false);
    ioManager->setPortCircuitTypes(toCircuitTypeByPort(portDescriptions));

    connect(
        this, &QnResource::propertyChanged,
        this, &IoModuleResource::at_propertyChanged,
        Qt::QueuedConnection);

    saveProperties();

    return CameraDiagnostics::NoErrorResult();
}

QString IoModuleResource::getDriverName() const
{
    return kHanwhaPoeNvrDriverName;
}

bool IoModuleResource::setOutputPortState(
    const QString& outputId, bool isActive, unsigned int autoResetTimeoutMs)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    nvr::IIoManager* const ioManager = getIoManager(serverModule());
    if (!ioManager)
        return false;

    const std::optional<QnIOPortData> portDescription = portDescriptionThreadUnsafe(outputId);
    if (!NX_ASSERT(portDescription))
        return false;

    return ioManager->setOutputPortState(
        portDescription->id,
        isActive ? IoPortState::active : IoPortState::inactive,
        std::chrono::milliseconds(autoResetTimeoutMs));
}

void IoModuleResource::startInputPortStatesMonitoring()
{
    nvr::IIoManager* const ioManager = getIoManager(serverModule());
    if (!NX_ASSERT(ioManager, "Unable to access IO manager while trying to register handler"))
        return;

    NX_MUTEX_LOCKER lock(&m_mutex);
    updatePortDescriptionsThreadUnsafe(ioPortDescriptions());
    m_initialilStateHasBeenProcessed = false;
    m_handlerId = ioManager->registerStateChangeHandler(
        [this](const QnIOStateDataList& state) { handleStateChange(state); });
}

void IoModuleResource::stopInputPortStatesMonitoring()
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    if (m_handlerId == 0)
    {
        NX_DEBUG(this, "Stopping input port monitoring, the state handler is not set");
        return;
    }

    nvr::IIoManager* const ioManager = getIoManager(serverModule());
    if (!NX_ASSERT(ioManager, "Unable to access IO manager while trying to unregister IO handler"))
        return;

    ioManager->unregisterStateChangeHandler(m_handlerId);
}

void IoModuleResource::updatePortDescriptionsThreadUnsafe(QnIOPortDataList portDescriptions)
{
    m_portDescriptions.clear();
    for (const QnIOPortData& portDescription: portDescriptions)
        m_portDescriptions[portDescription.id] = portDescription;
}

void IoModuleResource::handleStateChange(const QnIOStateDataList& portStates)
{
    NX_DEBUG(this, "Handling port state change, %1", containerString(portStates));

    struct PortStateWithType
    {
        QnIOStateData portState;
        Qn::IOPortType portType = Qn::IOPortType::PT_Unknown;
    };

    std::vector<PortStateWithType> changedPorts;
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        for (const QnIOStateData& portState: portStates)
        {
            if (!m_initialilStateHasBeenProcessed && !portState.isActive)
                continue;

            const std::optional<QnIOPortData> portDescription =
                portDescriptionThreadUnsafe(portState.id);

            if (!NX_ASSERT(portDescription))
                continue;

            changedPorts.push_back({portState, portDescription->portType});
        }
        m_initialilStateHasBeenProcessed = true;
    }

    for (const PortStateWithType& portStateWithType: changedPorts)
        emitSignals(portStateWithType.portState, portStateWithType.portType);
}

std::optional<QnIOPortData> IoModuleResource::portDescriptionThreadUnsafe(
    const QString& portId) const
{
    const auto logDescription =
        [this](const QnIOPortData& description)
        {
            NX_DEBUG(this, "Got port descriptor. Port id: %1, port type: %2, "
                "output default state: %3, input default state: %4",
                description.id, description.portType,
                description.oDefaultState, description.iDefaultState);
        };

    if (portId.isEmpty())
    {
        for (const auto& [_, portDescription]: m_portDescriptions)
        {
            if (portDescription.portType == Qn::IOPortType::PT_Output)
            {
                logDescription(portDescription);
                return portDescription;
            }
        }

        NX_WARNING(this, "Unable to find an output port description");
        return std::nullopt;
    }

    if (const auto it = m_portDescriptions.find(portId); it != m_portDescriptions.cend())
    {
        const QnIOPortData& description = it ->second;
        logDescription(description);

        return description;
    }

    NX_WARNING(this, "Unable to find port descriptor with id %1", portId);
    return std::nullopt;
}

void IoModuleResource::emitSignals(const QnIOStateData& portState, Qn::IOPortType portType)
{
    switch (portType)
    {
        case Qn::IOPortType::PT_Input:
        {
            NX_DEBUG(this,
                "Emitting inputPortStateChanged signal for port %1, state %2, timestamp %3 us",
                portState.id, portState.isActive, portState.timestamp);

            emit inputPortStateChanged(
                toSharedPointer(this),
                portState.id,
                portState.isActive,
                portState.timestamp);

            break;
        }
        case Qn::IOPortType::PT_Output:
        {
            NX_DEBUG(this,
                "Emitting outputPortStateChanged signal for port %1, state %2, timestamp %3 us",
                portState.id, portState.isActive, portState.timestamp);

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

QnIOPortDataList IoModuleResource::mergedPortDescriptions()
{
    const nvr::IIoManager* const ioManager = getIoManager(serverModule());
    if (!NX_ASSERT(ioManager))
        return {};

    std::map<QString, QnIOPortData> overridenPortDescriptorsById;
    for (QnIOPortData& descriptor: ioPortDescriptions())
        overridenPortDescriptorsById[descriptor.id] = std::move(descriptor);

    const QnIOPortDataList overridenPortDescriptors = ioPortDescriptions();

    QnIOPortDataList result;
    for (const QnIOPortData& descriptor: ioManager->portDesriptiors())
    {
        const auto it = overridenPortDescriptorsById.find(descriptor.id);
        if (it == overridenPortDescriptorsById.cend())
            result.push_back(descriptor);
        else
            result.push_back(it->second);
    }

    return result;
}

void IoModuleResource::at_propertyChanged(const QnResourcePtr& /*resource*/, const QString& key)
{
    if (key != ResourcePropertyKey::kIoSettings)
        return;

    NX_MUTEX_LOCKER lock(&m_mutex);
    const QnIOPortDataList portDescriptions = ioPortDescriptions();
    updatePortDescriptionsThreadUnsafe(portDescriptions);

    nvr::IIoManager* const ioManager = getIoManager(serverModule());
    if (!NX_ASSERT(ioManager))
        return;

    ioManager->setPortCircuitTypes(toCircuitTypeByPort(portDescriptions));
}

} // namespace nx::vms::server::nvr::hanwha
