// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "intercom.h"

#include <api/model/api_ioport_data.h>
#include <api/server_rest_connection.h>
#include <client_core/client_core_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource/user_resource.h>
#include <nx/reflect/json.h>
#include <nx/utils/std/algorithm.h>
#include <nx/vms/api/data/event_rule_data.h>
#include <nx/vms/client/core/resource/session_resources_signal_listener.h>
#include <nx/vms/common/resource/analytics_engine_resource.h>
#include <nx/vms/common/resource/analytics_plugin_resource.h>
#include <nx/vms/event/action_parameters.h>
#include <nx/vms/event/event_parameters.h>
#include <nx/vms/event/rule.h>
#include <nx/vms/event/rule_manager.h>
#include <nx_ec/abstract_ec_connection.h>
#include <nx_ec/data/api_conversion_functions.h>
#include <nx_ec/managers/abstract_event_rules_manager.h>

using namespace std::chrono;

namespace {

const QString kCreateLayoutRuleIsInitializedPropertyName =
    "intercomDemoCreateLayoutRuleIsInitialized";
const QString kCreateLayoutRuleIsInitializedPropertyValue = "true";

const QString kOpenDoorRuleIsInitializedPropertyName = "intercomDemoOpenDoorRuleIsInitialized";
const QString kOpenDoorRuleIsInitializedPropertyValue = "true";

const QString kCallEvent = "nx.sys.CallRequest";
const QnUuid kOpenLayoutRuleId = QnUuid::fromArbitraryData(
    std::string("nx.sys.IntercomIntegrationOpenLayout"));
const std::string kOpenDoorRuleIdBase = "nx.sys.IntercomIntegrationOpenDoor";
const QString kOpenDoorIcon = "_door_opened";
const QString kOpenDoorPortName =
    QString::fromStdString(nx::reflect::toString(nx::vms::api::ExtendedCameraOutput::powerRelay));
constexpr milliseconds kOpenedDoorDuration = 6s;

} // namespace

namespace nx::vms::client::desktop::integrations {

struct IntercomIntegration::Private: public QObject
{
    IntercomIntegration* const q = nullptr;

    QnUserResourcePtr currentUser;

    Private(IntercomIntegration* q):
        q(q)
    {
        auto camerasListener = new core::SessionResourcesSignalListener<
            QnVirtualCameraResource>(this);
        camerasListener->setOnAddedHandler(
            [this](const QnVirtualCameraResourceList& cameras)
            {
                tryCreateRules(cameras);
            });
        camerasListener->addOnSignalHandler(
            &QnVirtualCameraResource::compatibleEventTypesMaybeChanged,
            [this](const QnVirtualCameraResourcePtr& camera)
            {
                tryCreateRules({camera});
            });

        camerasListener->start();

        auto currentUserListener = new core::SessionResourcesSignalListener<QnUserResource>(this);
        currentUserListener->setOnRemovedHandler(
            [this](const QnUserResourceList& users)
            {
                if (currentUser && users.contains(currentUser))
                    currentUser.reset();
            });
        currentUserListener->start();
    }

    void responseHandler(ec2::Result result) const
    {
        if (result.error != ec2::ErrorCode::ok)
            NX_WARNING(this, result.message);
    }

    QnVirtualCameraResourceList getAliveIntercomCameras(
        const QnVirtualCameraResourceList& cameras) const
    {
        return cameras.filtered(
            [](const QnVirtualCameraResourcePtr& camera)
            {
                const std::map<QnUuid, std::set<QString>> allSupportedEvents =
                    camera->supportedEventTypes();

                if (std::none_of(allSupportedEvents.cbegin(), allSupportedEvents.cend(),
                    [](std::pair<QnUuid, std::set<QString>> eventsByEngineId)
                    {
                        return eventsByEngineId.second.count(kCallEvent) > 0;
                    }))
                {
                    return false;
                }

                const auto portDescriptions = camera->ioPortDescriptions();
                const auto portIter = std::find_if(
                    portDescriptions.begin(),
                    portDescriptions.end(),
                    [](const auto& portData) { return portData.outputName == kOpenDoorPortName; });

                if (portIter == portDescriptions.end())
                    return false;

                if (camera->flags().testFlag(Qn::ResourceFlag::removed))
                    return false;

                return true;
            });
    }

    void tryCreateRules(const QnVirtualCameraResourceList& cameras) const
    {
        // TODO: #dfisenko Delete this function and its dependencies.
#if 0
        if (!currentUser)
            return;

        const auto role = currentUser->userRole();
        const bool hasAdminPermissions = role == Qn::UserRole::owner
            || role == Qn::UserRole::administrator;
        if (!hasAdminPermissions)
            return;

        auto connection = this->q->messageBusConnection();
        if (!connection)
            return;

        const ec2::AbstractEventRulesManagerPtr manager =
            connection->getEventRulesManager(Qn::kSystemAccess);
        if (!manager)
            return;

        const QnVirtualCameraResourceList intercomCameras = getAliveIntercomCameras(cameras);
        if (intercomCameras.isEmpty())
            return;

        auto createRulesIfHandlerDoesNotExist =
            [this, manager, intercomCameras](
                int /*handle*/,
                ec2::ErrorCode errorCode,
                const nx::vms::api::EventRuleDataList& rules)
            {
                if (errorCode != ec2::ErrorCode::ok)
                    return;

                tryCreateAlarmLayoutRule(manager, intercomCameras, rules);
                tryCreateOpenDoorRule(manager, intercomCameras, rules);
            };

        manager->getEventRules(createRulesIfHandlerDoesNotExist, q);
#endif
    }

    void tryCreateAlarmLayoutRule(
        const ec2::AbstractEventRulesManagerPtr& manager,
        const QnVirtualCameraResourceList& cameras,
        const nx::vms::api::EventRuleDataList& rules) const
    {
        QnVirtualCameraResourceList newCameras;
        std::copy_if(cameras.begin(), cameras.end(), std::back_inserter(newCameras),
            [](const QnVirtualCameraResourcePtr& camera)
            {
                return camera->getProperty(kCreateLayoutRuleIsInitializedPropertyName)
                    != kCreateLayoutRuleIsInitializedPropertyValue;
            });

        if (newCameras.isEmpty())
            return;

        const nx::vms::event::RuleManager* localRuleManager =
            qnClientCoreModule->commonModule()->systemContext()->eventRuleManager();
        const nx::vms::event::RulePtr rule = localRuleManager->rule(kOpenLayoutRuleId);

        for (const auto& camera: newCameras)
        {
            camera->setProperty(
                kCreateLayoutRuleIsInitializedPropertyName,
                kCreateLayoutRuleIsInitializedPropertyValue);
            camera->savePropertiesAsync();
        }

        if (!rule)
        {
            manager->save(createOpenAlarmLayoutRule(newCameras),
                [this](int /*reqId*/, ec2::ErrorCode result) { responseHandler(result); });
            return;
        }

        const auto eventResources = rule->eventResources();
        const QSet<QnUuid> eventCameraIds(eventResources.begin(), eventResources.end());

        QSet<QnUuid> newCameraIds;
        for (const auto& camera: newCameras)
            newCameraIds.insert(camera->getId());

        const QSet<QnUuid> finalCameraIds = eventCameraIds + newCameraIds;

        nx::vms::api::EventRuleData updatedRuleData;
        ec2::fromResourceToApi(rule, updatedRuleData);
        updatedRuleData.eventResourceIds =
            std::vector<QnUuid>(finalCameraIds.begin(), finalCameraIds.end());
        manager->save(updatedRuleData,
            [this](int /*reqId*/, ec2::Result result) { responseHandler(result); });
    }

    std::vector<QnUuid> getCameraIdList(const QnVirtualCameraResourceList& cameras) const
    {
        std::vector<QnUuid> cameraIds(cameras.size());
        std::transform(cameras.begin(), cameras.end(), cameraIds.begin(),
            [](const QnVirtualCameraResourcePtr& camera) { return camera->getId(); });
        return cameraIds;
    }

    nx::vms::api::EventRuleData createOpenAlarmLayoutRule(
        const QnVirtualCameraResourceList& cameras) const
    {
        nx::vms::api::EventRuleData rule;
        rule.id = kOpenLayoutRuleId;
        rule.eventType = nx::vms::api::EventType::analyticsSdkEvent;
        rule.eventState = vms::api::EventState::active;
        rule.actionType = nx::vms::api::ActionType::showOnAlarmLayoutAction;
        rule.aggregationPeriod = 0;
        rule.eventResourceIds = getCameraIdList(cameras);
        // rule.actionResourceIds is empty for "useSource == true"
        rule.comment = tr("Intercom Alarm Layout");

        nx::vms::event::EventParameters eventParameters;
        eventParameters.setAnalyticsEventTypeId(kCallEvent);

        const auto supportedEvents = cameras.first()->supportedEventTypes();
        auto iter = std::find_if(supportedEvents.begin(), supportedEvents.end(),
            [](const auto& engineEvents) { return engineEvents.second.count(kCallEvent) > 0; });
        if (NX_ASSERT(iter != supportedEvents.end(), "Unexpected camera supported events list."))
            eventParameters.setAnalyticsEngineId(iter->first);

        eventParameters.metadata.allUsers = true;
        rule.eventCondition =
            QByteArray::fromStdString(nx::reflect::json::serialize(eventParameters));

        nx::vms::event::ActionParameters actionParameters;
        actionParameters.forced = true;
        actionParameters.allUsers = true;
        actionParameters.useSource = true;
        rule.actionParams =
            QByteArray::fromStdString(nx::reflect::json::serialize(actionParameters));

        return rule;
    }

    QnUuid getOpenDoorRuleId(const QnVirtualCameraResourcePtr& camera) const
    {
        return QnUuid::fromArbitraryData(kOpenDoorRuleIdBase + camera->getId().toStdString());
    }

    void tryCreateOpenDoorRule(
        const ec2::AbstractEventRulesManagerPtr& manager,
        const QnVirtualCameraResourceList& cameras,
        const nx::vms::api::EventRuleDataList& rules) const
    {
        const nx::vms::event::RuleManager* localRuleManager =
            qnClientCoreModule->commonModule()->systemContext()->eventRuleManager();

        for (const auto& camera: cameras)
        {
            if (camera->getProperty(kOpenDoorRuleIsInitializedPropertyName)
                == kOpenDoorRuleIsInitializedPropertyValue)
            {
                continue;
            }

            const QnUuid openDoorRuleId = getOpenDoorRuleId(camera);
            nx::vms::event::RulePtr rule = localRuleManager->rule(openDoorRuleId);

            if (!rule)
            {
                camera->setProperty(
                    kOpenDoorRuleIsInitializedPropertyName,
                    kOpenDoorRuleIsInitializedPropertyValue);
                camera->savePropertiesAsync();

                manager->save(createOpenDoorRule(camera),
                    [this](int /*reqId*/, ec2::ErrorCode result) { responseHandler(result); });
            }
        }
    }

    nx::vms::api::EventRuleData createOpenDoorRule(
        const QnVirtualCameraResourcePtr& camera) const
    {
        nx::vms::api::EventRuleData rule;
        rule.id = getOpenDoorRuleId(camera);
        rule.eventType = nx::vms::api::EventType::softwareTriggerEvent;
        rule.actionType = nx::vms::api::ActionType::cameraOutputAction;
        rule.aggregationPeriod = 0;
        rule.eventResourceIds = {camera->getId()};
        rule.actionResourceIds = {camera->getId()};
        rule.comment = tr("Intercom Open Door");

        nx::vms::event::EventParameters eventParameters;
        eventParameters.caption = IntercomIntegration::tr("Open door");
        eventParameters.description = kOpenDoorIcon;
        eventParameters.metadata.allUsers = true;
        rule.eventCondition =
            QByteArray::fromStdString(nx::reflect::json::serialize(eventParameters));

        nx::vms::event::ActionParameters actionParameters;

        const auto portDescriptions = camera->ioPortDescriptions();
        const auto portIter = std::find_if(portDescriptions.begin(), portDescriptions.end(),
            [](const auto& portData) { return portData.outputName == kOpenDoorPortName; } );
        if (NX_ASSERT(portIter != portDescriptions.end(), "Door open output port is absent."))
            actionParameters.relayOutputId = portIter->id;

        actionParameters.durationMs = kOpenedDoorDuration.count();
        rule.actionParams =
            QByteArray::fromStdString(nx::reflect::json::serialize(actionParameters));

        return rule;
    }
}; // struct IntercomIntegration::Private

IntercomIntegration::IntercomIntegration(QObject* parent):
    base_type(parent),
    d(new Private(this))
{
}

IntercomIntegration::~IntercomIntegration()
{
}

void IntercomIntegration::connectionEstablished(
    const QnUserResourcePtr& currentUser,
    ec2::AbstractECConnectionPtr connection)
{
    d->currentUser = currentUser;

    const auto cameras = currentUser->resourcePool()->getAllCameras();
    d->tryCreateRules(cameras);
}

} // namespace nx::vms::client::desktop::integrations
