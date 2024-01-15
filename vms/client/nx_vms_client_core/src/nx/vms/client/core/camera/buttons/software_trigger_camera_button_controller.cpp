// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "software_trigger_camera_button_controller.h"

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
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/resource/server.h>
#include <nx/vms/client/core/system_context.h>
#include <nx/vms/client/core/watchers/server_time_watcher.h>
#include <nx/vms/common/user_management/user_management_helpers.h>
#include <nx/vms/event/rule.h>
#include <nx/vms/event/rule_manager.h>
#include <nx/vms/event/strings_helper.h>
#include <nx/vms/rules/engine.h>
#include <nx/vms/rules/event_filter_fields/unique_id_field.h>
#include <nx/vms/rules/event_filter.h>
#include <nx/vms/rules/event_filter_fields/customizable_icon_field.h>
#include <nx/vms/rules/event_filter_fields/customizable_text_field.h>
#include <nx/vms/rules/event_filter_fields/source_camera_field.h>
#include <nx/vms/rules/event_filter_fields/source_user_field.h>
#include <nx/vms/rules/event_filter_fields/state_field.h>
#include <nx/vms/rules/events/soft_trigger_event.h>
#include <nx/vms/rules/ini.h>
#include <nx/vms/rules/rule.h>
#include <nx/vms/rules/utils.h>
#include <nx/vms/rules/utils/action.h>
#include <nx/vms/rules/utils/field.h>
#include <nx/utils/std/algorithm.h>
#include <nx/utils/qt_helpers.h>
#include <utils/common/delayed.h>
#include <utils/common/synctime.h>

#include "intercom_helper.h"

namespace nx::vms::client::core {

namespace {

/**
 * Returns true if rule relates to the 5.0 simplified intercom "open door" functionality, otherwise
 * false.
 * In 5.0 intercom open door is implemented as automatically created software trigger. Since in 5.1
 * we changed implementation using extendedOutput property - we need to filter it out.
 */
bool filterOutOutdatedOpenDoorTrigger(const nx::vms::event::RulePtr& rule)
{
    if (!rule)
        return true;

    const auto& eventResources = rule->eventResources();
    const auto cameraId = eventResources.empty()
        ? QnUuid()
        : eventResources.front();
    return rule->id() == nx::vms::client::core::IntercomHelper::intercomOpenDoorRuleId(cameraId);
}

bool isSuitableRule(
    const nx::vms::event::RulePtr& rule,
    const QnUserResourcePtr& currentUser,
    const QnUuid& resourceId)
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
    QnUuid resourceId)
{
    if (!rule->enabled())
        return false;

    // TODO: #amalov support multiple event rule logic.
    const auto filter = rule->eventFilters()[0];
    if (filter->eventType() != nx::vms::rules::SoftTriggerEvent().manifest().id)
        return false;

    const auto cameraField = filter->fieldByName<nx::vms::rules::SourceCameraField>(
        nx::vms::rules::utils::kCameraIdFieldName);
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

using HintStyle = SoftwareTriggerCameraButtonController::HintStyle;
QString buttonHint(bool prolonged, HintStyle hintStyle)
{
    if (!prolonged)
        return {};

    return hintStyle == HintStyle::mobile
        ? SoftwareTriggerCameraButtonController::tr("Press and hold to") + ' '
        : SoftwareTriggerCameraButtonController::tr("press and hold");
}

CameraButton::Type prolongedToType(bool prolonged)
{
    return prolonged
        ? CameraButton::Type::prolonged
        : CameraButton::Type::instant;
}

CameraButton buttonFromRule(
    const nx::vms::event::RulePtr& rule,
    SystemContext* /*context*/,
    HintStyle hintStyle)
{
    const auto params = rule->eventParams();
    const bool prolonged = rule->isActionProlonged();
    return CameraButton {
        .id = rule->id(),
        .name = vms::event::StringsHelper::getSoftwareTriggerName(params.caption),
        .hint = buttonHint(prolonged, hintStyle),
        .iconName = params.description,
        .type = prolongedToType(prolonged),
        .enabled = true};
}

CameraButton buttonFromRule(
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
    return CameraButton {
        .id = rule->id(),
        .name = StringsHelper::getSoftwareTriggerName(name->value()),
        .hint = buttonHint(prolonged, hintStyle),
        .iconName = icon->value(),
        .type = prolongedToType(prolonged),
        .enabled = true};
}

} // namespace

//-------------------------------------------------------------------------------------------------

struct SoftwareTriggerCameraButtonController::Private
{
    SoftwareTriggerCameraButtonController * const q;
    const HintStyle hintStyle;

    QSet<QnUuid> isVmsRuleFlags;
    ServerResourcePtr server;

    void updateButtons();
    void addOrUpdateButtonData(const CameraButton& button, bool isVmsRule);
    void tryRemoveButton(const QnUuid& buttonId);

    template<typename RulePointerType>
    void updateButtonByRule(RulePointerType rule);

    void updateButtonAvailability(CameraButton button);

    void handleCameraUpdated();

    bool setEventTriggerState(const QnUuid& ruleId, vms::event::EventState state);
    bool setVmsTriggerState(const QnUuid& ruleId, vms::event::EventState state);
    void updateActiveTrigger(const QnUuid& ruleId, vms::event::EventState state, bool success);
};

void SoftwareTriggerCameraButtonController::Private::updateButtons()
{
    auto removedIds =
        [this]()
        {
            QnUuidSet result;
            for (const auto& button: q->buttons())
                result.insert(button.id);
            return result;
        }();

    const auto handleRule =
        [this, &removedIds](auto rule)
        {
            /**
             * We should not remove any processed rule since the button for it either has been
             * removed or added/updated already.
             */
            updateButtonByRule(rule);
            removedIds.remove(rule->id());
        };

    for (const auto& rule: q->systemContext()->eventRuleManager()->rules())
        handleRule(rule);

    if (nx::vms::rules::ini().fullSupport)
    {
        for (const auto& [_, rule]: q->systemContext()->vmsRulesEngine()->rules())
            handleRule(rule.get());
    }

    for (const auto& id: removedIds)
        tryRemoveButton(id);
}

void SoftwareTriggerCameraButtonController::Private::addOrUpdateButtonData(
    const CameraButton& button,
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

void SoftwareTriggerCameraButtonController::Private::tryRemoveButton(const QnUuid& buttonId)
{
    if (q->removeButton(buttonId))
        isVmsRuleFlags.remove(buttonId);
}

template<typename RulePointerType>
void SoftwareTriggerCameraButtonController::Private::updateButtonByRule(RulePointerType rule)
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

void SoftwareTriggerCameraButtonController::Private::updateButtonAvailability(CameraButton button)
{
    // FIXME: #sivanov System-wide timezone should be used here when it is introduced.
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

void SoftwareTriggerCameraButtonController::Private::handleCameraUpdated()
{
    const auto& camera = q->camera();
    server = camera
        ? camera->getParentServer().dynamicCast<ServerResource>()
        : ServerResourcePtr();

    // TODO: handle parent server (parentIdChanged) signal
    updateButtons();
}

bool SoftwareTriggerCameraButtonController::Private::setEventTriggerState(
    const QnUuid& ruleId,
    vms::event::EventState state)
{
    const auto rule = q->systemContext()->eventRuleManager()->rule(ruleId);
    if (!NX_ASSERT(rule, "Trigger does not exist"))
        return false;

    const auto callback = nx::utils::guarded(q,
        [this, state, ruleId](
            bool success, rest::Handle /*handle*/, const nx::network::rest::JsonResult& result)
        {
            success = success && result.error == nx::network::rest::Result::NoError;
            updateActiveTrigger(ruleId, state, success);
        });

    const auto eventParams = rule->eventParams();
    nx::network::rest::Params params;
    params.insert("timestamp", QString::number(qnSyncTime->currentMSecsSinceEpoch()));
    params.insert("event_type",
        nx::reflect::toString(nx::vms::api::EventType::softwareTriggerEvent));
    params.insert("inputPortId", eventParams.inputPortId);
    params.insert("eventResourceId", q->resourceId().toString());
    params.insert("caption", eventParams.getTriggerName());
    params.insert("description", eventParams.getTriggerIcon());

    if (state != nx::vms::api::EventState::undefined)
        params.insert("state", nx::reflect::toString(state));

    const auto handle = q->connectedServerApi()->postJsonResult(
        "/api/createEvent", params, {}, callback, QThread::currentThread());

    return handle != -1;
}

bool SoftwareTriggerCameraButtonController::Private::setVmsTriggerState(
    const QnUuid& ruleId,
    vms::event::EventState state)
{
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

    auto triggerEvent = QSharedPointer<nx::vms::rules::SoftTriggerEvent>::create(
        qnSyncTime->currentTimePoint(),
        nx::vms::rules::convertEventState(state),
        idField->id(),
        q->resourceId(),
        q->systemContext()->accessController()->user()->getId(),
        nx::vms::event::StringsHelper::getSoftwareTriggerName(nameField->value()),
        iconField->value());

    q->systemContext()->vmsRulesEngine()->processEvent(triggerEvent);

    const auto completionHandler =
        [this, ruleId, state]() { updateActiveTrigger(ruleId, state, /*success*/ true); };
    executeLater(completionHandler , q);

    return true;
}

void SoftwareTriggerCameraButtonController::Private::updateActiveTrigger(
    const QnUuid& ruleId,
    vms::event::EventState state,
    bool success)
{
    if (state == nx::vms::event::EventState::inactive)
        q->safeEmitActionStopped(ruleId, success);
    else
        q->safeEmitActionStarted(ruleId, success);
}

//-------------------------------------------------------------------------------------------------

SoftwareTriggerCameraButtonController::SoftwareTriggerCameraButtonController(
    HintStyle hintStyle,
    CameraButton::Group buttonGroup,
    SystemContext* context,
    QObject* parent)
    :
    base_type(buttonGroup, context, Qn::SoftTriggerPermission, parent),
    d(new Private{.q = this, .hintStyle = hintStyle})
{
    auto ruleManager = context->eventRuleManager();

    const auto updateButtons = [this]() { d->updateButtons(); };
    const auto tryRemoveButton = [this](const QnUuid& id) { d->tryRemoveButton(id); };
    const auto updateButtonByRule = [this](auto rule) { d->updateButtonByRule(rule); };

    connect(ruleManager, &vms::event::RuleManager::rulesReset, this, updateButtons);
    connect(ruleManager, &vms::event::RuleManager::ruleRemoved, this, tryRemoveButton);
    connect(ruleManager, &vms::event::RuleManager::ruleAddedOrUpdated, this, updateButtonByRule);

    if (nx::vms::rules::ini().fullSupport)
    {
        auto engine = context->vmsRulesEngine();

        connect(engine, &nx::vms::rules::Engine::rulesReset, this, updateButtons);
        connect(engine, &nx::vms::rules::Engine::ruleRemoved, this, tryRemoveButton);
        connect(engine, &nx::vms::rules::Engine::ruleAddedOrUpdated, this,
            [engine, updateButtonByRule](auto id) { updateButtonByRule(engine->rule(id).get()); });
    }

    connect(this, &BaseCameraButtonController::hasRequiredPermissionsChanged, this, updateButtons);

    connect(context->resourcePool(), &QnResourcePool::resourcesRemoved, this,
        [this](const QnResourceList& resources)
        {
            if (d->server && resources.contains(d->server))
                d->server.clear();
        });

    connect(this, &SoftwareTriggerCameraButtonController::cameraChanged, this,
        [this]() { d->handleCameraUpdated(); });

    static const auto kUpdateTriggerAvailabilityInterval = std::chrono::seconds(1);
    const auto updateTimer(new QTimer(this));
    updateTimer->setInterval(kUpdateTriggerAvailabilityInterval);
    connect(updateTimer, &QTimer::timeout, this,
        [this]()
        {
            for (const auto& button: buttons())
                d->updateButtonAvailability(button);
        });
    updateTimer->start();
}

SoftwareTriggerCameraButtonController::~SoftwareTriggerCameraButtonController()
{
}

bool SoftwareTriggerCameraButtonController::setButtonActionState(
    const CameraButton& button,
    ActionState state)
{
    if (!hasRequiredPermissions())
        return false;

    const auto eventState =
        [state, button]()
        {
            if (state == ActionState::inactive)
                return vms::event::EventState::inactive;

            return button.instant()
                ? vms::event::EventState::undefined
                : vms::event::EventState::active;
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
