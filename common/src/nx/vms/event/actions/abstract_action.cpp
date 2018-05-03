#include "abstract_action.h"

#include <QtCore/QCoreApplication>

#include <nx/vms/event/strings_helper.h>
#include <nx/fusion/model_functions.h>

namespace nx {
namespace vms {
namespace event {

ActionData::ActionData(const ActionData& src):
    actionType(src.actionType),
    actionParams(src.actionParams),
    eventParams(src.eventParams),
    businessRuleId(src.businessRuleId),
    aggregationCount(src.aggregationCount),
    flags(src.flags),
    compareString(src.compareString)
{
    NX_EXPECT(false, "ActionData must never be copied. Constructor must exist up to C++17. "
        "See forced NRVO in the server_rest_connection.cpp pipml (parseMessageBody(), "
        "'deserialized' method call).");
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
        case ActionType::openLayoutAction:
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
        case ActionType::playSoundOnceAction:
        case ActionType::playSoundAction:
        case ActionType::sayTextAction:
        case ActionType::executePtzPresetAction:
        case ActionType::showTextOverlayAction:
        case ActionType::showOnAlarmLayoutAction:
        case ActionType::execHttpRequestAction:
        case ActionType::openLayoutAction:
            return false;

        case ActionType::acknowledgeAction:
        case ActionType::sendMailAction:
            return true;

        default:
            NX_ASSERT(false, Q_FUNC_INFO, "All action types must be handled.");
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
            return false;

        case ActionType::bookmarkAction:
        case ActionType::showPopupAction:
        case ActionType::playSoundOnceAction:
        case ActionType::sayTextAction:
        case ActionType::showOnAlarmLayoutAction:
            return true;

        default:
            NX_ASSERT(false, Q_FUNC_INFO, "All action types must be handled.");
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
        case ActionType::playSoundOnceAction:
        case ActionType::sayTextAction:
        case ActionType::executePtzPresetAction:
        case ActionType::showOnAlarmLayoutAction:
        case ActionType::execHttpRequestAction:
        case ActionType::acknowledgeAction:
        case ActionType::openLayoutAction:
            return false;

        case ActionType::cameraOutputAction:
        case ActionType::cameraRecordingAction:
        case ActionType::panicRecordingAction:
        case ActionType::playSoundAction:
        case ActionType::bookmarkAction:
        case ActionType::showTextOverlayAction:
            return true;

        default:
            NX_ASSERT(false, Q_FUNC_INFO, "All action types must be handled.");
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
            return false;

        default:
            return true;
    }
}

bool isActionProlonged(ActionType actionType, const ActionParameters &parameters)
{
    if (!hasToggleState(actionType))
        return false;

    switch (actionType)
    {
        case ActionType::bookmarkAction:
        case ActionType::showTextOverlayAction:
        case ActionType::cameraOutputAction:
        case ActionType::cameraRecordingAction:
            return parameters.durationMs <= 0;

        default:
            break;
    }

    return true;
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
        ActionType::playSoundAction,
        ActionType::playSoundOnceAction,
        ActionType::sayTextAction,
        ActionType::executePtzPresetAction,
        ActionType::showTextOverlayAction,
        ActionType::showOnAlarmLayoutAction,
        ActionType::execHttpRequestAction,
        ActionType::openLayoutAction,
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

QVector<QnUuid> AbstractAction::getSourceResources() const
{
    NX_ASSERT(m_params.useSource, Q_FUNC_INFO, "Method should be called only when corresponding parameter is set.");
    QVector<QnUuid> result;
    result << m_runtimeParams.eventResourceId;
    for (const QnUuid &extra : m_runtimeParams.metadata.cameraRefs)
        if (!result.contains(extra))
            result << extra;
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
    return lit("action_") + QString::number(static_cast<int>(m_actionType)) + L'_';
}

void AbstractAction::assign(const AbstractAction& other)
{
    (*this) = other;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (ActionData), (ubjson)(json)(xml)(csv_record), _Fields)

} // namespace event
} // namespace vms
} // namespace nx
