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
        case undefinedAction:
        case panicRecordingAction:
        case sendMailAction:
        case diagnosticsAction:
        case showPopupAction:
        case openLayoutAction:
            return false;

        case playSoundOnceAction:
        case playSoundAction:
        case sayTextAction:
        case cameraOutputAction:
        case bookmarkAction:
        case cameraRecordingAction:
        case executePtzPresetAction:
        case showTextOverlayAction:
        case showOnAlarmLayoutAction:
        case acknowledgeAction:
            return true;

        default:
            return false;
    }
}

bool requiresUserResource(ActionType actionType)
{
    switch (actionType)
    {
        case undefinedAction:
        case panicRecordingAction:
        case cameraOutputAction:
        case bookmarkAction:
        case cameraRecordingAction:
        case diagnosticsAction:
        case showPopupAction:
        case playSoundOnceAction:
        case playSoundAction:
        case sayTextAction:
        case executePtzPresetAction:
        case showTextOverlayAction:
        case showOnAlarmLayoutAction:
        case execHttpRequestAction:
        case openLayoutAction:
            return false;

        case acknowledgeAction:
        case sendMailAction:
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
        case undefinedAction:
        case panicRecordingAction:
        case cameraOutputAction:
        case cameraRecordingAction:
        case diagnosticsAction:
        case playSoundAction:
        case executePtzPresetAction:
        case showTextOverlayAction:
        case execHttpRequestAction:
        case acknowledgeAction:
        case sendMailAction:
        case openLayoutAction:
            return false;

        case bookmarkAction:
        case showPopupAction:
        case playSoundOnceAction:
        case sayTextAction:
        case showOnAlarmLayoutAction:
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
        case undefinedAction:
        case sendMailAction:
        case diagnosticsAction:
        case showPopupAction:
        case playSoundOnceAction:
        case sayTextAction:
        case executePtzPresetAction:
        case showOnAlarmLayoutAction:
        case execHttpRequestAction:
        case acknowledgeAction:
        case openLayoutAction:
            return false;

        case cameraOutputAction:
        case cameraRecordingAction:
        case panicRecordingAction:
        case playSoundAction:
        case bookmarkAction:
        case showTextOverlayAction:
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
        case bookmarkAction:
        case showTextOverlayAction:
        case cameraOutputAction:
        case cameraRecordingAction:
            return true;
        default:
            return false;
    }
}

bool allowsAggregation(ActionType actionType)
{
    switch (actionType)
    {
        case bookmarkAction:
        case showTextOverlayAction:
        case cameraOutputAction:
        case playSoundAction:
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
        case bookmarkAction:
        case showTextOverlayAction:
        case cameraOutputAction:
        case cameraRecordingAction:
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
        cameraOutputAction,
        bookmarkAction,
        cameraRecordingAction,
        panicRecordingAction,
        sendMailAction,
        diagnosticsAction,
        showPopupAction,
        playSoundAction,
        playSoundOnceAction,
        sayTextAction,
        executePtzPresetAction,
        showTextOverlayAction,
        showOnAlarmLayoutAction,
        execHttpRequestAction,
        openLayoutAction,
    };

    return result;
}

QList<ActionType> allActions()
{
    static QList<ActionType> result =
        []()
        {
            QList<ActionType> result = userAvailableActions();
            result.append(acknowledgeAction);
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
