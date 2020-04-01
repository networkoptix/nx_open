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

    const nvr::IIoManager* const ioManager = getIoManager(serverModule());
    if (!ioManager)
        return CameraDiagnostics::IOErrorResult("Unable to access NVR IO manager");

    setIoPortDescriptions(mergedPortDescriptions(), /*needMerge*/ false);
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

    const std::optional<QnIOPortData> descriptor = portDescriptionThreadUnsafe(
        outputId, Qn::IOPortType::PT_Output);

    if (!NX_ASSERT(descriptor))
        return false;

    return ioManager->setOutputPortState(
        descriptor->id,
        isActiveTranslated(*descriptor, isActive) ? IoPortState::active : IoPortState::inactive,
        std::chrono::milliseconds(autoResetTimeoutMs));
}

void IoModuleResource::startInputPortStatesMonitoring()
{
    nvr::IIoManager* const ioManager = getIoManager(serverModule());
    if (!NX_ASSERT(ioManager, "Unable to access IO manager while trying to register handler"))
        return;

    QnIOStateDataList portStates;
    std::map<QString, QnIOPortData> portDescriptions;

    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        updatePortDescriptionsThreadUnsafe(ioPortDescriptions());

        portStates = currentPortStatesThreadUnsafe();
        portDescriptions = m_portDescriptorsById;

        m_handlerId = ioManager->registerStateChangeHandler(
            [this](const QnIOStateDataList& state) { handleStateChange(state); });
    }

    for (const QnIOStateData& portState: portStates)
    {
        if (portState.isActive)
            emitSignals(portState, portDescriptions[portState.id].portType);
    }
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

void IoModuleResource::handleStateChange(const QnIOStateDataList& portStates)
{
    std::vector<PortStateNotification> changedStateNotifications;

    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        changedStateNotifications = updatePortStatesThreadUnsafe(portStates);
    }

    for (const PortStateNotification& notification: changedStateNotifications)
        emitSignals(notification.portState, notification.portType);
}

std::optional<QnIOPortData> IoModuleResource::portDescriptionThreadUnsafe(
    const QString& portId, Qn::IOPortType portType) const
{
    const auto logDescriptor =
        [this](const QnIOPortData& descriptor)
        {
            NX_DEBUG(this, "Got port descriptor. Port id: %1, port type: %2, "
                "output default state: %3, input default state: %4",
                descriptor.id, descriptor.portType,
                descriptor.oDefaultState, descriptor.iDefaultState);
        };

    if (portId.isEmpty() && portType != Qn::IOPortType::PT_Unknown)
    {
        for (const auto& [portId, descriptor]: m_portDescriptorsById)
        {
            if (descriptor.portType == portType)
            {
                logDescriptor(descriptor);
                return descriptor;
            }
        }
    }

    if (const auto it = m_portDescriptorsById.find(portId); it != m_portDescriptorsById.cend())
    {
        const QnIOPortData& descriptor = it ->second;
        logDescriptor(descriptor);

        return descriptor;
    }

    NX_WARNING(this, "Unable to find port descriptor with id %1 and port type: %2",
        portId, portType);
    return std::nullopt;
}

std::vector<IoModuleResource::PortStateNotification>
    IoModuleResource::updatePortStatesThreadUnsafe(const QnIOStateDataList& state)
{
    NX_DEBUG(this, "Handling IO port state change, IO state: %1", containerString(state));

    std::vector<PortStateNotification> changedStateNotifications;
    for (const QnIOStateData& portState: state)
    {
        const std::optional<QnIOPortData> portDescriptor =
            portDescriptionThreadUnsafe(portState.id);

        if (!NX_ASSERT(portDescriptor))
            continue;

        QnIOStateData newPortState;
        newPortState.id = portState.id;
        newPortState.timestamp = portState.timestamp;
        newPortState.isActive = isActiveTranslated(*portDescriptor, portState.isActive);

        const QnIOStateData currentPortState = this->currentPortStateThreadUnsafe(portState.id);

        if (!NX_ASSERT(newPortState.id == currentPortState.id))
            continue;

        m_portStateById[newPortState.id] = newPortState;

        if (newPortState.isActive != currentPortState.isActive)
        {
            NX_DEBUG(this, "State of the port %1 has been changed to %2",
                newPortState.id, newPortState.isActive);

            changedStateNotifications.push_back({
                std::move(newPortState),
                portDescriptor->portType});
        }
    }

    return changedStateNotifications;
}

void IoModuleResource::updatePortDescriptionsThreadUnsafe(QnIOPortDataList portDescriptors)
{
    NX_DEBUG(this, "Got port descriptors: %1", containerString(portDescriptors));

    m_portDescriptorsById.clear();
    for (const QnIOPortData& descriptor: portDescriptors)
        m_portDescriptorsById.emplace(descriptor.id, descriptor);

    updatePortStatesThreadUnsafe(getIoManager(serverModule())->portStates());
}

QnIOStateData IoModuleResource::currentPortStateThreadUnsafe(const QString& portId) const
{
    if (const auto it = m_portStateById.find(portId); it != m_portStateById.cend())
    {
        const QnIOStateData& portState = it->second;
        NX_DEBUG(this, "Current port state (found in cache) of the port %1: is active: %2",
            portState.id, portState.isActive);

        return portState;
    }

    QnIOStateData result;
    result.id = portId;

    const std::optional<QnIOPortData> descriptor = portDescriptionThreadUnsafe(portId);
    if (!NX_ASSERT(descriptor))
        return result;

    result.isActive = isActiveTranslated(*descriptor, result.isActive);
    result.timestamp = qnSyncTime->currentUSecsSinceEpoch();

    NX_DEBUG(this, "Current port state of the port %1: is active: %2", result.id, result.isActive);

    return result;
}

QnIOStateDataList IoModuleResource::currentPortStatesThreadUnsafe() const
{
    QnIOStateDataList portStates;
    for (const auto& [portId, _]: m_portDescriptorsById)
        portStates.push_back(currentPortStateThreadUnsafe(portId));

    return portStates;
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

bool IoModuleResource::isActiveTranslated(const QnIOPortData& portDescription, bool isActive) const
{
    if (!NX_ASSERT(portDescription.portType == Qn::IOPortType::PT_Input
        || portDescription.portType == Qn::IOPortType::PT_Output))
    {
        return false;
    }

    const Qn::IODefaultState defaultState = (portDescription.portType == Qn::IOPortType::PT_Input)
        ? portDescription.iDefaultState
        : portDescription.oDefaultState;

    const bool isActiveTranslated = (defaultState == Qn::IODefaultState::IO_OpenCircuit)
        ? isActive
        : !isActive;

    NX_DEBUG(this, "Translating port state for port %1, port type: %2, default input state: %3, "
        "default output state %4, is active (raw): %5, is active (translated): %6",
        portDescription.id, portDescription.portType,
        portDescription.iDefaultState, portDescription.oDefaultState,
        isActive, isActiveTranslated);

    return isActiveTranslated;
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

    std::map<QString, QnIOStateData> previousPortStates;
    std::map<QString, QnIOStateData> newPortStates;
    std::map<QString, QnIOPortData> newPortDescriptions;

    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        previousPortStates = m_portStateById;
        updatePortDescriptionsThreadUnsafe(ioPortDescriptions());
        newPortStates = m_portStateById;
        newPortDescriptions = m_portDescriptorsById;
    }

    for (const auto& [portId, newPortState]: newPortStates)
    {
        if (newPortState.isActive != previousPortStates[portId].isActive)
        {
            NX_DEBUG(this, "State of the port %1 has been changed to %2",
                portId, newPortState.isActive);

            emitSignals(newPortState, newPortDescriptions[portId].portType);
        }
    }
}

} // namespace nx::vms::server::nvr::hanwha
