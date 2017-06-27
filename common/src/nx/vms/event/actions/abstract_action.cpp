#include "abstract_action.h"

#include <QtCore/QCoreApplication>

#include <nx/vms/event/strings_helper.h>
#include <nx/fusion/model_functions.h>

namespace nx {
namespace vms {
namespace event {

bool requiresCameraResource(ActionType actionType)
{
    switch (actionType)
    {
        case UndefinedAction:
        case PanicRecordingAction:
        case SendMailAction:
        case DiagnosticsAction:
        case ShowPopupAction:
            return false;

        case PlaySoundOnceAction:
        case PlaySoundAction:
        case SayTextAction:
        case CameraOutputAction:
        case BookmarkAction:
        case CameraRecordingAction:
        case ExecutePtzPresetAction:
        case ShowTextOverlayAction:
        case ShowOnAlarmLayoutAction:
            return true;

        default:
            return false;
    }
}

bool requiresUserResource(ActionType actionType)
{
    switch (actionType)
    {
        case UndefinedAction:
        case PanicRecordingAction:
        case CameraOutputAction:
        case BookmarkAction:
        case CameraRecordingAction:
        case DiagnosticsAction:
        case ShowPopupAction:
        case PlaySoundOnceAction:
        case PlaySoundAction:
        case SayTextAction:
        case ExecutePtzPresetAction:
        case ShowTextOverlayAction:
        case ShowOnAlarmLayoutAction:
        case ExecHttpRequestAction:
            return false;

        case SendMailAction:
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
        case UndefinedAction:
        case SendMailAction:
        case DiagnosticsAction:
        case ShowPopupAction:
        case PlaySoundOnceAction:
        case SayTextAction:
        case ExecutePtzPresetAction:
        case ShowOnAlarmLayoutAction:
            return false;
        case ExecHttpRequestAction:
            return false;

        case CameraOutputAction:
        case CameraRecordingAction:
        case PanicRecordingAction:
        case PlaySoundAction:
        case BookmarkAction:
        case ShowTextOverlayAction:
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
        case BookmarkAction:
        case ShowTextOverlayAction:
        case CameraOutputAction:
        case CameraRecordingAction:
            return true;
        default:
            return false;
    }
}

bool allowsAggregation(ActionType actionType)
{
    switch (actionType)
    {
        case BookmarkAction:
        case ShowTextOverlayAction:
        case CameraOutputAction:
        case PlaySoundAction:
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
        case BookmarkAction:
        case ShowTextOverlayAction:
        case CameraOutputAction:
        case CameraRecordingAction:
            return parameters.durationMs <= 0;

        default:
            break;
    }

    return true;
}

QList<ActionType> allActions()
{
    static QList<ActionType> result {
        CameraOutputAction,
        BookmarkAction,
        CameraRecordingAction,
        PanicRecordingAction,
        SendMailAction,
        DiagnosticsAction,
        ShowPopupAction,
        PlaySoundAction,
        PlaySoundOnceAction,
        SayTextAction,
        ExecutePtzPresetAction,
        ShowTextOverlayAction,
        ShowOnAlarmLayoutAction,
        ExecHttpRequestAction };

    return result;
}

AbstractAction::AbstractAction(const ActionType actionType, const EventParameters& runtimeParams):
    m_actionType(actionType),
    m_toggleState(UndefinedState),
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

void AbstractAction::setActionType(ActionType actionType)
{
    m_actionType = actionType;
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
