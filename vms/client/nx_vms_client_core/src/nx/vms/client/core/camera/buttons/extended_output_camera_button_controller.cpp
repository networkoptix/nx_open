// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "extended_output_camera_button_controller.h"

#include <api/server_rest_connection.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/scoped_connections.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/data/event_rule_data.h>
#include <nx/vms/client/core/camera/buttons/intercom_helper.h>
#include <nx/vms/client/core/camera/iomodule/io_module_monitor.h>
#include <nx/vms/client/core/io_ports/io_ports_compatibility_interface.h>
#include <nx/vms/client/core/system_context.h>
#include <nx/vms/event/action_parameters.h>

namespace nx::vms::client::core {

namespace {

using ExtendedCameraOutput = nx::vms::api::ExtendedCameraOutput;
using ExtendedCameraOutputs = nx::vms::api::ExtendedCameraOutputs;

using ButtonsMap = QMap<ExtendedCameraOutput, CameraButtonData>;
using OutputsSet = QSet<ExtendedCameraOutput>;
using OutputNameToIdMap = QMap<QString, QString>;

const QString kAutoTrackingPortName = QString::fromStdString(
    nx::reflect::toString(nx::vms::api::ExtendedCameraOutput::autoTracking));

CameraButtonData button(const QString& name, const QString& iconName)
{
    static const QString kAdvancedCameraButtonPrefix = "advanced_camera_button";

    return {
        .id = nx::Uuid::fromArbitraryData(kAdvancedCameraButtonPrefix + name + iconName),
        .name = name,
        .iconName = iconName,
        .enabled = true
    };
}

static const std::map<ExtendedCameraOutput, CameraButtonData> supportedOutputTypeToButtons = {
    {ExtendedCameraOutput::heater,
        button(ExtendedOutputCameraButtonController::tr("Heater"), "heater")},
    {ExtendedCameraOutput::wiper,
        button(ExtendedOutputCameraButtonController::tr("Wiper"), "wiper")},
    {ExtendedCameraOutput::powerRelay,
        button(ExtendedOutputCameraButtonController::tr("Open Door"), "_door_opened")},
    {ExtendedCameraOutput::autoTracking,
        button(ExtendedOutputCameraButtonController::tr("Stop Object Tracking"), "object_tracking_stop")}};

static const auto supportedButtonToOutputType =
    []()
    {
        std::map<nx::Uuid, ExtendedCameraOutput> result;
        for (const auto& [outputType, button]: supportedOutputTypeToButtons)
            result.emplace(button.id, outputType);
        return result;
    }();

OutputsSet outputSetFromFlags(ExtendedCameraOutputs value)
{
    OutputsSet result;
    for (const auto& [outputType, _]: supportedOutputTypeToButtons)
    {
        if (value.testFlag(outputType))
            result += outputType;
    }
    return result;
}

} // namespace

struct ExtendedOutputCameraButtonController::Private
{
    ExtendedOutputCameraButtonController * const q;
    const OutputsSet allowedOutputs;
    OutputsSet outputs;
    OutputNameToIdMap outputNameToId;
    IOModuleMonitorPtr ioModuleMonitor;
    bool activeObjectTracking = false;
    nx::utils::ScopedConnections connections;

    void updateOutputs();
    void openIoModuleConnectionIfNeeded();
    void updateSupportAutoTrackingHandling();
    void setActiveObjectTracking(bool value);
    QString outputId(ExtendedCameraOutput outputType) const;
};

void ExtendedOutputCameraButtonController::Private::updateOutputs()
{
    const auto newOutputs =
        [this]()
        {
            const auto camera = q->camera();
            OutputsSet result;
            if (!camera || !q->hasRequiredPermissions())
                return result;

            result = outputSetFromFlags(camera->extendedOutputs()).intersect(allowedOutputs);
            const auto it = result.find(ExtendedCameraOutput::autoTracking);
            if (it != result.end() && !activeObjectTracking)
                result.erase(it);

            return result;
        }();

    if (outputs == newOutputs)
        return;

    const auto addedOutputs = newOutputs - outputs;
    const auto removedOutputs = outputs - newOutputs;

    outputNameToId =
        [this]()
        {
            OutputNameToIdMap result;
            const auto camera = q->camera();
            if (!camera)
                return result;

            for (const QnIOPortData& portData: camera->ioPortDescriptions())
                result[portData.outputName] = portData.id;

            return result;
        }();

    outputs = newOutputs;

    for (const auto& outputType: removedOutputs)
        q->removeButton(supportedOutputTypeToButtons.at(outputType).id);

    for (const auto& outputType: addedOutputs)
        q->addOrUpdateButton(supportedOutputTypeToButtons.at(outputType));
}

void ExtendedOutputCameraButtonController::Private::updateSupportAutoTrackingHandling()
{
    const auto camera = q->camera();
    const QnIOPortDataList& ports = camera
        ? camera->ioPortDescriptions()
        : QnIOPortDataList{};

    const bool supportAutoTracking = std::ranges::any_of(
        ports,
        [](const auto& port)
        {
            return port.outputName == kAutoTrackingPortName;
        });

    if (supportAutoTracking == !!ioModuleMonitor)
        return;

    setActiveObjectTracking(false); //< Reset state and wait for io module update.

    if (!supportAutoTracking)
    {
        ioModuleMonitor.reset();
        return;
    }

    ioModuleMonitor.reset(new IOModuleMonitor(camera));
    connect(ioModuleMonitor.get(), &IOModuleMonitor::ioStateChanged, q,
        [this](const QnIOStateData& value)
        {
            if (value.id == outputId(ExtendedCameraOutput::autoTracking))
                setActiveObjectTracking(value.isActive);
        });
    ioModuleMonitor->open();
}

void ExtendedOutputCameraButtonController::Private::setActiveObjectTracking(bool value)
{
    if (activeObjectTracking == value)
        return;

    activeObjectTracking = value;
    updateOutputs();
}

QString ExtendedOutputCameraButtonController::Private::outputId(ExtendedCameraOutput outputType) const
{
    const auto it = outputNameToId.find(
        QString::fromStdString(nx::reflect::toString(outputType)));

    return it == outputNameToId.end()
        ? QString{}
        : it.value();
}

//-------------------------------------------------------------------------------------------------

ExtendedOutputCameraButtonController::ExtendedOutputCameraButtonController(
    ExtendedCameraOutputs allowedOutputs,
    CameraButtonData::Group buttonGroup,
    QObject* parent)
    :
    base_type(buttonGroup, Qn::DeviceInputPermission, parent),
    d(new Private{.q = this, .allowedOutputs = outputSetFromFlags(allowedOutputs)})
{
}

ExtendedOutputCameraButtonController::~ExtendedOutputCameraButtonController()
{
}

void ExtendedOutputCameraButtonController::setResourceInternal(const QnResourcePtr& resource)
{
    d->connections.reset();
    d->ioModuleMonitor.reset();
    d->setActiveObjectTracking(false);

    base_type::setResourceInternal(resource);

    d->updateOutputs();

    if (!resource)
        return;

    d->updateSupportAutoTrackingHandling();
    connect(resource.get(), &QnResource::propertyChanged, this,
        [this](const QnResourcePtr& /*resource*/,
            const QString& key,
            const QString& /*prevValue*/,
            const QString& /*newValue*/)
        {
            if (key == nx::vms::api::device_properties::kCameraCapabilities
                || key == nx::vms::api::device_properties::kTwoWayAudioEnabled
                || key == nx::vms::api::device_properties::kAudioOutputDeviceId
                || key == nx::vms::api::device_properties::kIoSettings)
            {
                d->updateOutputs();
                d->updateSupportAutoTrackingHandling();
            }
        });
}

bool ExtendedOutputCameraButtonController::setButtonActionState(
    const CameraButtonData& button,
    ActionState state)
{
    if (state == ActionState::inactive)
        return false;

    const auto targetCamera = camera();
    if (!targetCamera)
        return false;

    const auto itOutputType = supportedButtonToOutputType.find(button.id);
    if (itOutputType == supportedButtonToOutputType.end())
        return false;

    const auto outputType = itOutputType->second;
    if (!d->outputs.contains(outputType))
        return false;

    const auto systemContext = camera()->systemContext()->as<SystemContext>();

    auto callback = nx::utils::guarded(this,
        [this, buttonId = button.id](bool success)
        {
            safeEmitActionStarted(buttonId, success);
        });

    if (!systemContext->ioPortsInterface())
        return false;

    return systemContext->ioPortsInterface()->setIoPortState(
        systemContext->getSessionTokenHelper(),
        camera(),
        outputType,
        /*isActive*/ outputType != ExtendedCameraOutput::autoTracking,
        /*autoResetTimeout*/ outputType == ExtendedCameraOutput::powerRelay
            ? IntercomHelper::kOpenedDoorDuration
            : std::chrono::milliseconds{},
        /*targetLockResolutionData*/ {},
        callback);
}

} // namespace nx::vms::client::core
