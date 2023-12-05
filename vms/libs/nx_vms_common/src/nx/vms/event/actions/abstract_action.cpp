// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "abstract_action.h"

#include <QtCore/QCoreApplication>

#include <api/helpers/camera_id_helper.h>
#include <core/resource/camera_resource.h>
#include <nx/fusion/model_functions.h>
#include <nx/vms/event/strings_helper.h>

namespace nx::vms::event {

ActionFlags getFlags(ActionType actionType)
{
    static const QMap<ActionType, ActionFlags> flags = {
        {ActionType::bookmarkAction, {ActionFlag::canUseSourceCamera}},
        {ActionType::cameraOutputAction, {ActionFlag::canUseSourceCamera}},
        {ActionType::cameraRecordingAction, {ActionFlag::canUseSourceCamera}},
        {ActionType::fullscreenCameraAction, {ActionFlag::canUseSourceCamera}},
        {ActionType::playSoundAction, {ActionFlag::canUseSourceCamera}},
        {ActionType::playSoundOnceAction, {ActionFlag::canUseSourceCamera}},
        {ActionType::sayTextAction, {ActionFlag::canUseSourceCamera}},
        {ActionType::showOnAlarmLayoutAction, {ActionFlag::canUseSourceCamera}},
        {ActionType::showTextOverlayAction, {ActionFlag::canUseSourceCamera}},
    };

    return flags.value(actionType);
}

bool requiresCameraResource(ActionType actionType)
{
    switch (actionType)
    {
        case ActionType::undefinedAction:
        case ActionType::panicRecordingAction:
        case ActionType::sendMailAction:
        case ActionType::diagnosticsAction:
        case ActionType::showPopupAction:
        case ActionType::pushNotificationAction:
        case ActionType::openLayoutAction:
        case ActionType::exitFullscreenAction:
        case ActionType::buzzerAction:
            return false;

        case ActionType::playSoundOnceAction:
        case ActionType::playSoundAction:
        case ActionType::sayTextAction:
        case ActionType::cameraOutputAction:
        case ActionType::bookmarkAction:
        case ActionType::cameraRecordingAction:
        case ActionType::executePtzPresetAction:
        case ActionType::showTextOverlayAction:
        case ActionType::showOnAlarmLayoutAction:
        case ActionType::acknowledgeAction:
        case ActionType::fullscreenCameraAction:
            return true;

        default:
            return false;
    }
}

bool requiresUserResource(ActionType actionType)
{
    switch (actionType)
    {
        case ActionType::undefinedAction:
        case ActionType::panicRecordingAction:
        case ActionType::cameraOutputAction:
        case ActionType::bookmarkAction:
        case ActionType::cameraRecordingAction:
        case ActionType::diagnosticsAction:
        case ActionType::showPopupAction:
        case ActionType::pushNotificationAction:
        case ActionType::playSoundOnceAction:
        case ActionType::playSoundAction:
        case ActionType::sayTextAction:
        case ActionType::executePtzPresetAction:
        case ActionType::showTextOverlayAction:
        case ActionType::showOnAlarmLayoutAction:
        case ActionType::execHttpRequestAction:
        case ActionType::openLayoutAction:
        case ActionType::fullscreenCameraAction:
        case ActionType::exitFullscreenAction:
        case ActionType::buzzerAction:
            return false;

        case ActionType::acknowledgeAction:
        case ActionType::sendMailAction:
            return true;

        default:
            NX_ASSERT(false, "All action types must be handled.");
            return false;
    }
}

// TODO: #vkutin #3.2 User resources and device resources of actions will be refactored.
bool requiresAdditionalUserResource(ActionType actionType)
{
    switch (actionType)
    {
        case ActionType::undefinedAction:
        case ActionType::panicRecordingAction:
        case ActionType::cameraOutputAction:
        case ActionType::cameraRecordingAction:
        case ActionType::diagnosticsAction:
        case ActionType::playSoundAction:
        case ActionType::executePtzPresetAction:
        case ActionType::showTextOverlayAction:
        case ActionType::execHttpRequestAction:
        case ActionType::acknowledgeAction:
        case ActionType::sendMailAction:
        case ActionType::openLayoutAction:
        case ActionType::fullscreenCameraAction:
        case ActionType::exitFullscreenAction:
        case ActionType::buzzerAction:
            return false;

        case ActionType::bookmarkAction:
        case ActionType::showPopupAction:
        case ActionType::pushNotificationAction:
        case ActionType::playSoundOnceAction:
        case ActionType::sayTextAction:
        case ActionType::showOnAlarmLayoutAction:
            return true;

        default:
            NX_ASSERT(false, "All action types must be handled.");
            return false;
    }
}

bool requiresServerResource(ActionType actionType)
{
    switch (actionType)
    {
        case ActionType::buzzerAction:
            return true;
        default:
            return false;
    }
}

bool hasToggleState(ActionType actionType)
{
    switch (actionType)
    {
        case ActionType::undefinedAction:
        case ActionType::sendMailAction:
        case ActionType::diagnosticsAction:
        case ActionType::showPopupAction:
        case ActionType::pushNotificationAction:
        case ActionType::playSoundOnceAction:
        case ActionType::sayTextAction:
        case ActionType::executePtzPresetAction:
        case ActionType::showOnAlarmLayoutAction:
        case ActionType::execHttpRequestAction:
        case ActionType::acknowledgeAction:
        case ActionType::openLayoutAction:
        case ActionType::fullscreenCameraAction:
        case ActionType::exitFullscreenAction:
            return false;

        case ActionType::cameraOutputAction:
        case ActionType::cameraRecordingAction:
        case ActionType::panicRecordingAction:
        case ActionType::playSoundAction:
        case ActionType::bookmarkAction:
        case ActionType::showTextOverlayAction:
        case ActionType::buzzerAction:
            return true;

        default:
            NX_ASSERT(false, "Unhandled action type: %1", actionType);
            break;
    }
    return false;
}

bool canBeInstant(ActionType actionType)
{
    if (!hasToggleState(actionType))
        return true;

    return supportsDuration(actionType);
}

bool supportsDuration(ActionType actionType)
{
    switch (actionType)
    {
        case ActionType::bookmarkAction:
        case ActionType::showTextOverlayAction:
        case ActionType::cameraOutputAction:
        case ActionType::cameraRecordingAction:
        case ActionType::buzzerAction:
            NX_ASSERT(hasToggleState(actionType),
                "Action %1 should have toggle state to support duration", actionType);
            return true;
        default:
            return false;
    }
}

bool allowsAggregation(ActionType actionType)
{
    switch (actionType)
    {
        case ActionType::bookmarkAction:
        case ActionType::showTextOverlayAction:
        case ActionType::cameraOutputAction:
        case ActionType::playSoundAction:
        case ActionType::fullscreenCameraAction:
        case ActionType::exitFullscreenAction:
            return false;

        default:
            return true;
    }
}

bool canUseSourceCamera(ActionType actionType)
{
    return getFlags(actionType).testFlag(ActionFlag::canUseSourceCamera);
}

bool isActionProlonged(ActionType actionType, const ActionParameters &parameters)
{
    if (supportsDuration(actionType))
        return parameters.durationMs <= 0;

    return hasToggleState(actionType);
}

QList<ActionType> userAvailableActions()
{
    static QList<ActionType> result
    {
        ActionType::cameraOutputAction,
        ActionType::bookmarkAction,
        ActionType::cameraRecordingAction,
        ActionType::panicRecordingAction,
        ActionType::sendMailAction,
        ActionType::diagnosticsAction,
        ActionType::showPopupAction,
        ActionType::pushNotificationAction,
        ActionType::playSoundAction,
        ActionType::playSoundOnceAction,
        ActionType::sayTextAction,
        ActionType::executePtzPresetAction,
        ActionType::showTextOverlayAction,
        ActionType::showOnAlarmLayoutAction,
        ActionType::execHttpRequestAction,
        ActionType::openLayoutAction,
        ActionType::fullscreenCameraAction,
        ActionType::exitFullscreenAction,
        ActionType::buzzerAction,
    };

    return result;
}

QList<ActionType> allActions()
{
    static QList<ActionType> result =
        []()
        {
            QList<ActionType> result = userAvailableActions();
            result.append(ActionType::acknowledgeAction);
            return result;
        }();

    return result;
}

AbstractAction::AbstractAction(const ActionType actionType, const EventParameters& runtimeParams):
    m_actionType(actionType),
    m_toggleState(EventState::undefined),
    m_receivedFromRemoteHost(false),
    m_runtimeParams(runtimeParams),
    m_aggregationCount(1)
{
}

AbstractAction::~AbstractAction()
{
}

ActionType AbstractAction::actionType() const
{
    return m_actionType;
}

void AbstractAction::setResources(const QVector<QnUuid>& resources)
{
    m_resources = resources;
}

const QVector<QnUuid>& AbstractAction::getResources() const
{
    return m_resources;
}

QVector<QnUuid> AbstractAction::getSourceResources(const QnResourcePool* resourcePool) const
{
    NX_ASSERT(m_params.useSource, "Method should be called only when corresponding parameter is set.");
    QVector<QnUuid> result;
    result << m_runtimeParams.eventResourceId;
    for (const auto& flexibleId: m_runtimeParams.metadata.cameraRefs)
    {
        auto camera = camera_id_helper::findCameraByFlexibleId(resourcePool, flexibleId);
        if (camera && !result.contains(camera->getId()))
            result.push_back(camera->getId());
    }
    return result;
}

void AbstractAction::setParams(const ActionParameters& params)
{
    m_params = params;
}

const ActionParameters& AbstractAction::getParams() const
{
    return m_params;
}

ActionParameters& AbstractAction::getParams()
{
    return m_params;
}

void AbstractAction::setRuntimeParams(const EventParameters& params)
{
    m_runtimeParams = params;
}

const EventParameters& AbstractAction::getRuntimeParams() const
{
    return m_runtimeParams;
}

EventParameters& AbstractAction::getRuntimeParams()
{
    return m_runtimeParams;
}

void AbstractAction::setRuleId(const QnUuid& value)
{
    m_ruleId = value;
}

QnUuid AbstractAction::getRuleId() const
{
    return m_ruleId;
}

void AbstractAction::setToggleState(EventState value)
{
    m_toggleState = value;
}

EventState AbstractAction::getToggleState() const
{
    return m_toggleState;
}

void AbstractAction::setReceivedFromRemoteHost(bool value)
{
    m_receivedFromRemoteHost = value;
}

bool AbstractAction::isReceivedFromRemoteHost() const
{
    return m_receivedFromRemoteHost;
}

int AbstractAction::getAggregationCount() const
{
    return m_aggregationCount;
}

bool AbstractAction::isProlonged() const
{
    return isActionProlonged(m_actionType, m_params);
}

void AbstractAction::setAggregationCount(int value)
{
    m_aggregationCount = value;
}

QString AbstractAction::getExternalUniqKey() const
{
    return QLatin1String("action_") + QString::number(static_cast<int>(m_actionType)) + '_';
}

void AbstractAction::assign(const AbstractAction& other)
{
    (*this) = other;
}

QString ActionData::toString() const
{
    return QJson::serialized(*this);
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ActionData, (ubjson)(json)(xml)(csv_record), ActionData_Fields)

} // namespace nx::vms::event
