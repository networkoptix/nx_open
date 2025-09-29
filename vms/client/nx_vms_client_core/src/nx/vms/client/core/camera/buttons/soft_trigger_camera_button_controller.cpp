// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "soft_trigger_camera_button_controller.h"

#include <api/server_rest_connection.h>
#include <core/resource/camera_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_access/global_permissions_watcher.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_access/resource_access_subject.h>
#include <core/resource_access/resource_access_subject_hierarchy.h>
#include <core/resource_management/resource_pool.h>
#include <nx/reflect/string_conversion.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/qobject.h>
#include <nx/utils/qt_helpers.h>
#include <nx/utils/scoped_connections.h>
#include <nx/utils/std/algorithm.h>
#include <nx/vms/api/rules/event_info.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/core/resource/server.h>
#include <nx/vms/client/core/system_context.h>
#include <nx/vms/client/core/watchers/server_time_watcher.h>
#include <nx/vms/common/user_management/user_management_helpers.h>
#include <nx/vms/event/rule.h>
#include <nx/vms/event/rule_manager.h>
#include <nx/vms/rules/engine.h>
#include <nx/vms/rules/event_filter.h>
#include <nx/vms/rules/event_filter_fields/customizable_icon_field.h>
#include <nx/vms/rules/event_filter_fields/customizable_text_field.h>
#include <nx/vms/rules/event_filter_fields/source_camera_field.h>
#include <nx/vms/rules/event_filter_fields/source_user_field.h>
#include <nx/vms/rules/event_filter_fields/state_field.h>
#include <nx/vms/rules/event_filter_fields/unique_id_field.h>
#include <nx/vms/rules/events/soft_trigger_event.h>
#include <nx/vms/rules/rule.h>
#include <nx/vms/rules/strings.h>
#include <nx/vms/rules/utils.h>
#include <nx/vms/rules/utils/action.h>
#include <nx/vms/rules/utils/api.h>
#include <nx/vms/rules/utils/field.h>
#include <utils/common/delayed.h>
#include <utils/common/synctime.h>

#include "intercom_helper.h"

namespace nx::vms::client::core {

namespace {

/**
 * Returns true if rule relates to the 5.0 simplified intercom "open door" functionality, otherwise
 * false.
 * In 5.0 intercom open door is implemented as automatically created soft trigger. Since in 5.1
 * we changed implementation using extendedOutput property - we need to filter it out.
 */
bool filterOutOutdatedOpenDoorTrigger(const nx::vms::event::RulePtr& rule)
{
    if (!rule)
        return true;

    const auto& eventResources = rule->eventResources();
    const auto cameraId = eventResources.empty()
        ? nx::Uuid()
        : eventResources.front();
    return rule->id() == nx::vms::client::core::IntercomHelper::intercomOpenDoorRuleId(cameraId);
}

bool isSuitableRule(
    const nx::vms::event::RulePtr& rule,
    const QnUserResourcePtr& currentUser,
    const nx::Uuid& resourceId)
{
    if (rule->isDisabled() || rule->eventType() != nx::vms::api::softwareTriggerEvent)
        return false;

    if (!rule->eventResources().empty() && !rule->eventResources().contains(resourceId))
        return false;

    if (filterOutOutdatedOpenDoorTrigger(rule))
        return false;

    if (rule->eventParams().metadata.allUsers)
        return true;

    const auto subjects = rule->eventParams().metadata.instigators;
    if (nx::utils::contains(subjects, currentUser->getId()))
        return true;

    const auto userGroups = nx::vms::common::userGroupsWithParents(currentUser);
    for (const auto& groupId: userGroups)
    {
        if (nx::utils::contains(subjects, groupId))
            return true;
    }

    return false;
}

bool isSuitableRule(
    const nx::vms::rules::Rule* rule,
    const QnUserResourcePtr& currentUser,
    nx::Uuid resourceId)
{
    if (!rule || !rule->enabled())
        return false;

    // TODO: #amalov support multiple event rule logic.
    const auto filter = rule->eventFilters()[0];
    if (filter->eventType() != nx::vms::rules::SoftTriggerEvent().manifest().id)
        return false;

    const auto cameraField = filter->fieldByName<nx::vms::rules::SourceCameraField>(
        nx::vms::rules::utils::kDeviceIdFieldName);
    const auto userField = filter->fieldByName<nx::vms::rules::SourceUserField>("userId");
    if (!NX_ASSERT(cameraField && userField))
        return false;

    if (!cameraField->acceptAll() && !cameraField->ids().contains(resourceId))
        return false;

    if (userField->acceptAll() || userField->ids().contains(currentUser->getId()))
        return true;

    const auto userGroups = nx::vms::common::userGroupsWithParents(currentUser);
    return userGroups.intersects(userField->ids());
}

bool isVmsRule(const nx::vms::event::RulePtr& /*rule*/)
{
    return false;
}

bool isVmsRule(const nx::vms::rules::Rule* /*rule*/)
{
    return true;
}

using HintStyle = SoftTriggerCameraButtonController::HintStyle;
QString buttonHint(bool prolonged, HintStyle hintStyle)
{
    if (!prolonged)
        return {};

    return hintStyle == HintStyle::mobile
        ? SoftTriggerCameraButtonController::tr("Press and hold to") + ' '
        : SoftTriggerCameraButtonController::tr("press and hold");
}

CameraButtonData::Type prolongedToType(bool prolonged)
{
    return prolonged
        ? CameraButtonData::Type::prolonged
        : CameraButtonData::Type::instant;
}

CameraButtonData buttonFromRule(
    const nx::vms::event::RulePtr& rule,
    SystemContext* /*context*/,
    HintStyle hintStyle)
{
    const auto params = rule->eventParams();
    const bool prolonged = rule->isActionProlonged();
    return CameraButtonData {
        .id = rule->id(),
        .name = nx::vms::rules::Strings::softTriggerName(params.caption),
        .hint = buttonHint(prolonged, hintStyle),
        .iconName = params.description,
        .type = prolongedToType(prolonged),
        .enabled = true};
}

CameraButtonData buttonFromRule(
    const nx::vms::rules::Rule* rule,
    SystemContext* context,
    HintStyle hintStyle)
{
    // TODO: #amalov Support multiple event logic.
    using namespace nx::vms::rules;
    using namespace nx::vms::event;

    const auto filter = rule->eventFilters()[0];
    const auto name = filter->fieldByName<CustomizableTextField>("triggerName");
    const auto icon = filter->fieldByName<CustomizableIconField>("triggerIcon");

    if (!NX_ASSERT(name && icon))
        return {};

    const auto engine = context->vmsRulesEngine();
    const bool prolonged = nx::vms::rules::isProlonged(engine, rule->actionBuilders()[0]);
    return CameraButtonData {
        .id = rule->id(),
        .name = nx::vms::rules::Strings::softTriggerName(name->value()),
        .hint = buttonHint(prolonged, hintStyle),
        .iconName = icon->value(),
        .type = prolongedToType(prolonged),
        .enabled = true};
}

bool isVmsRulesSupported(SystemContext* context)
{
    static constexpr auto kVmsRulesVersion = nx::utils::SoftwareVersion(6, 1);

    return context->moduleInformation().version >= kVmsRulesVersion
        && context->vmsRulesEngine(); //< Cross-system contexts currently don't have it.
}

} // namespace

//-------------------------------------------------------------------------------------------------

struct SoftTriggerCameraButtonController::Private
{
    SoftTriggerCameraButtonController * const q;
    const HintStyle hintStyle;

    QSet<nx::Uuid> isVmsRuleFlags;
    nx::utils::ScopedConnections connections;

    void updateButtons();
    void addOrUpdateButtonData(const CameraButtonData& button, bool isVmsRule);
    void tryRemoveButton(const nx::Uuid& buttonId);

    template<typename RulePointerType>
    void updateButtonByRule(RulePointerType rule);

    void updateButtonAvailability(CameraButtonData button);

    bool setEventTriggerState(const nx::Uuid& ruleId, vms::api::EventState state);
    bool setVmsTriggerState(const nx::Uuid& ruleId, vms::api::EventState state);
    void updateActiveTrigger(const nx::Uuid& ruleId, vms::api::EventState state, bool success);
};

void SoftTriggerCameraButtonController::Private::updateButtons()
{
    auto removedIds =
        [this]()
        {
            UuidSet result;
            for (const auto& button: q->buttonsData())
                result.insert(button.id);
            return result;
        }();

    const auto handleRule =
        [this, &removedIds](const auto& rule)
        {
            /**
             * We should not remove any processed rule since the button for it either has been
             * removed or added/updated already.
             */
            updateButtonByRule(rule);
            removedIds.remove(rule->id());
        };

    if (q->resource())
    {
        if (isVmsRulesSupported(q->systemContext()))
        {
            for (const auto& [_, rule]: q->systemContext()->vmsRulesEngine()->rules())
                handleRule(rule.get());
        }
        else
        {
            for (const auto& rule: q->systemContext()->eventRuleManager()->rules())
                handleRule(rule);
        }
    }

    for (const auto& id: removedIds)
        tryRemoveButton(id);
}

void SoftTriggerCameraButtonController::Private::addOrUpdateButtonData(
    const CameraButtonData& button,
    bool isVmsRule)
{
    if (q->addOrUpdateButton(button))
    {
        if (isVmsRule)
            isVmsRuleFlags.insert(button.id);
        else
            isVmsRuleFlags.remove(button.id);
    }

    updateButtonAvailability(button);
}

void SoftTriggerCameraButtonController::Private::tryRemoveButton(const nx::Uuid& buttonId)
{
    if (q->removeButton(buttonId))
        isVmsRuleFlags.remove(buttonId);
}

template<typename RulePointerType>
void SoftTriggerCameraButtonController::Private::updateButtonByRule(RulePointerType rule)
{
    const auto context = q->systemContext();
    const auto currentUser = context->accessController()->user();
    const bool toUpdate = q->hasRequiredPermissions()
        && isSuitableRule(rule, currentUser, q->camera()->getId());

    if (toUpdate)
        addOrUpdateButtonData(buttonFromRule(rule, context, hintStyle), isVmsRule(rule));
    else
        tryRemoveButton(rule->id());
}

void SoftTriggerCameraButtonController::Private::updateButtonAvailability(CameraButtonData button)
{
    // FIXME: #sivanov System-wide timezone should be used here when it is introduced.

    const auto camera = q->camera();
    if (!camera)
        return;

    const auto server = camera
        ? camera->getParentResource().objectCast<ServerResource>()
        : ServerResourcePtr{};

    const QTimeZone tz = NX_ASSERT(server)
        ? server->timeZone()
        : QTimeZone::LocalTime;

    const auto dateTime = QDateTime::fromMSecsSinceEpoch(qnSyncTime->currentMSecsSinceEpoch(), tz);

    const auto context = q->systemContext();
    bool enabled = button.enabled;
    if (isVmsRuleFlags.contains(button.id))
    {
        if (const auto rule = context->vmsRulesEngine()->rule(button.id))
            enabled = rule->timeInSchedule(dateTime);
        else
            return;
    }
    else
    {
        if (const auto rule = context->eventRuleManager()->rule(button.id))
            enabled = rule->isScheduleMatchTime(dateTime);
        else
            return;
    }

    if (button.enabled == enabled)
        return;

    button.enabled = enabled;
    q->addOrUpdateButton(button);
}

bool SoftTriggerCameraButtonController::Private::setEventTriggerState(
    const nx::Uuid& ruleId,
    vms::api::EventState state)
{
    const auto connection = q->systemContext()->connection();
    if (!connection)
        return false;

    const auto api = connection->serverApi();
    if (!NX_ASSERT(api))
        return false;

    const auto rule = q->systemContext()->eventRuleManager()->rule(ruleId);
    if (!NX_ASSERT(rule, "Trigger does not exist"))
        return false;

    const auto callback = nx::utils::guarded(q,
        [this, state, ruleId](
            bool success, rest::Handle /*handle*/, const nx::network::rest::JsonResult& result)
        {
            success = success && result.errorId == nx::network::rest::ErrorId::ok;
            updateActiveTrigger(ruleId, state, success);
        });

    const auto eventParams = rule->eventParams();
    nx::network::rest::Params params;
    params.insert("timestamp", QString::number(qnSyncTime->currentMSecsSinceEpoch()));
    params.insert("event_type",
        nx::reflect::toString(nx::vms::api::EventType::softwareTriggerEvent));
    params.insert("inputPortId", eventParams.inputPortId);
    params.insert("eventResourceId", q->resource()->getId().toString(QUuid::WithBraces));
    params.insert("caption", eventParams.getTriggerName());
    params.insert("description", eventParams.getTriggerIcon());

    if (state != nx::vms::api::EventState::undefined)
        params.insert("state", nx::reflect::toString(state));

    static constexpr auto kAction = "/api/createEvent";
    const auto thread = QThread::currentThread();
    const rest::Handle handle = connection->isRestApiSupported()
        ? api->postJsonResult(kAction, params, /*body*/ {}, callback, thread)
        : api->getJsonResult(kAction, params, callback, thread);

    return handle != rest::Handle{};
}

bool SoftTriggerCameraButtonController::Private::setVmsTriggerState(
    const nx::Uuid& ruleId,
    vms::api::EventState state)
{
    auto api = q->systemContext()->connectedServerApi();
    if (!api)
        return false;

    const auto rule = q->systemContext()->vmsRulesEngine()->rule(ruleId);
    if (!NX_ASSERT(rule, "Trigger does not exist"))
        return false;

    const auto filter = rule->eventFilters()[0];
    const auto idField = filter->fieldByName<nx::vms::rules::UniqueIdField>("triggerId");
    const auto nameField =
        filter->fieldByName<nx::vms::rules::CustomizableTextField>("triggerName");
    const auto iconField =
        filter->fieldByName<nx::vms::rules::CustomizableIconField>("triggerIcon");
    if (!NX_ASSERT(idField && nameField && iconField, "Invalid event manifest"))
        return false;

    auto triggerData = nx::vms::api::rules::SoftTriggerData{
        .triggerId = idField->id(),
        .deviceId = q->resource()->getId(),
        .state = nx::vms::rules::convertEventState(state),
    };

    api->createSoftTrigger(triggerData,
        [this, ruleId, state](bool success, auto /*handle*/, auto result)
        {
            if (!result)
            {
                NX_ERROR(this, "Can't activate soft trigger, rule: %1, code: %2, error: %3",
                    ruleId, result.error().errorId, result.error().errorString);
            }

            updateActiveTrigger(ruleId, state, success);
        },
        q->thread());

    return true;
}

void SoftTriggerCameraButtonController::Private::updateActiveTrigger(
    const nx::Uuid& ruleId,
    vms::api::EventState state,
    bool success)
{
    if (state == nx::vms::api::EventState::inactive)
        q->safeEmitActionStopped(ruleId, success);
    else
        q->safeEmitActionStarted(ruleId, success);
}

//-------------------------------------------------------------------------------------------------

SoftTriggerCameraButtonController::SoftTriggerCameraButtonController(
    HintStyle hintStyle,
    CameraButtonData::Group buttonGroup,
    QObject* parent)
    :
    base_type(buttonGroup, Qn::SoftTriggerPermission, parent),
    d(new Private{.q = this, .hintStyle = hintStyle})
{
    connect(this, &BaseCameraButtonController::hasRequiredPermissionsChanged, this,
        [this]() { d->updateButtons(); });

    static const auto kUpdateTriggerAvailabilityInterval = std::chrono::seconds(1);
    const auto updateTimer(new QTimer(this));
    updateTimer->setInterval(kUpdateTriggerAvailabilityInterval);
    connect(updateTimer, &QTimer::timeout, this,
        [this]()
        {
            for (const auto& button: buttonsData())
                d->updateButtonAvailability(button);
        });
    updateTimer->start();
}

SoftTriggerCameraButtonController::~SoftTriggerCameraButtonController()
{
}

void SoftTriggerCameraButtonController::setResourceInternal(const QnResourcePtr& resource)
{
    d->connections.reset();
    base_type::setResourceInternal(resource);

    if (!resource)
        return;

    auto systemContext = SystemContext::fromResource(resource);
    if (!NX_ASSERT(systemContext))
        return;

    const auto updateButtons = [this]() { d->updateButtons(); };
    const auto tryRemoveButton = [this](const nx::Uuid& id) { d->tryRemoveButton(id); };
    const auto updateButtonByRule = [this](auto rule) { d->updateButtonByRule(rule); };

    if (isVmsRulesSupported(systemContext))
    {
        auto engine = systemContext->vmsRulesEngine();
        d->connections << connect(
            engine, &nx::vms::rules::Engine::rulesReset, this, updateButtons);
        d->connections << connect(
            engine, &nx::vms::rules::Engine::ruleRemoved, this, tryRemoveButton);
        d->connections << connect(
            engine, &nx::vms::rules::Engine::ruleAddedOrUpdated, this,
            [engine, updateButtonByRule](auto id) { updateButtonByRule(engine->rule(id).get()); });
    }
    else
    {
        auto ruleManager = systemContext->eventRuleManager();

        d->connections << connect(
            ruleManager, &vms::event::RuleManager::rulesReset, this, updateButtons);
        d->connections << connect(
            ruleManager, &vms::event::RuleManager::ruleRemoved, this, tryRemoveButton);
        d->connections << connect(
            ruleManager, &vms::event::RuleManager::ruleAddedOrUpdated, this, updateButtonByRule);
    }

    d->updateButtons();
}

bool SoftTriggerCameraButtonController::setButtonActionState(
    const CameraButtonData& button,
    ActionState state)
{
    if (!hasRequiredPermissions())
        return false;

    const auto eventState =
        [state, button]()
        {
            if (state == ActionState::inactive)
                return vms::api::EventState::inactive;

            return button.instant()
                ? vms::api::EventState::undefined
                : vms::api::EventState::active;
        }();

    const auto& targetCamera = camera();
    if (!NX_ASSERT(targetCamera))
        return false;

    const auto result = d->isVmsRuleFlags.contains(button.id)
        ? d->setVmsTriggerState(button.id, eventState)
        : d->setEventTriggerState(button.id, eventState);

    if (result && !button.instant())
    {
        if (state == ActionState::active)
            addActiveAction(button.id);
        else
            removeActiveAction(button.id);
    }

    return result;
}

} // namespace nx::vms::client::core
