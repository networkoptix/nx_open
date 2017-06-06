#include "abstract_business_action.h"

#include <QtCore/QCoreApplication>

#include <business/business_strings_helper.h>
#include <nx/fusion/model_functions.h>

namespace QnBusiness {
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

bool isActionProlonged(ActionType actionType, const QnBusinessActionParameters &parameters)
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
    QList<ActionType> result;
    result
        << CameraOutputAction
        << BookmarkAction
        << CameraRecordingAction
        << PanicRecordingAction
        << SendMailAction
        << DiagnosticsAction
        << ShowPopupAction
        << PlaySoundAction
        << PlaySoundOnceAction
        << SayTextAction
        << ExecutePtzPresetAction
        << ShowTextOverlayAction
        << ShowOnAlarmLayoutAction
        << ExecHttpRequestAction
        ;
    return result;
}
}

QnAbstractBusinessAction::QnAbstractBusinessAction(const QnBusiness::ActionType actionType, const QnBusinessEventParameters& runtimeParams) :
    m_actionType(actionType),
    m_toggleState(QnBusiness::UndefinedState),
    m_receivedFromRemoteHost(false),
    m_runtimeParams(runtimeParams),
    m_aggregationCount(1)
{}

QnAbstractBusinessAction::~QnAbstractBusinessAction()
{}

QnBusiness::ActionType QnAbstractBusinessAction::actionType() const
{
    return m_actionType;
}

void QnAbstractBusinessAction::setResources(const QVector<QnUuid>& resources)
{
    m_resources = resources;
}

const QVector<QnUuid>& QnAbstractBusinessAction::getResources() const
{
    return m_resources;
}

QVector<QnUuid> QnAbstractBusinessAction::getSourceResources() const
{
    NX_ASSERT(m_params.useSource, Q_FUNC_INFO, "Method should be called only when corresponding parameter is set.");
    QVector<QnUuid> result;
    result << m_runtimeParams.eventResourceId;
    for (const QnUuid &extra : m_runtimeParams.metadata.cameraRefs)
        if (!result.contains(extra))
            result << extra;
    return result;
}

void QnAbstractBusinessAction::setParams(const QnBusinessActionParameters& params)
{
    m_params = params;
}

const QnBusinessActionParameters& QnAbstractBusinessAction::getParams() const
{
    return m_params;
}

QnBusinessActionParameters& QnAbstractBusinessAction::getParams()
{
    return m_params;
}

void QnAbstractBusinessAction::setRuntimeParams(const QnBusinessEventParameters& params)
{
    m_runtimeParams = params;
}

const QnBusinessEventParameters& QnAbstractBusinessAction::getRuntimeParams() const
{
    return m_runtimeParams;
}

void QnAbstractBusinessAction::setBusinessRuleId(const QnUuid& value)
{
    m_businessRuleId = value;
}

QnUuid QnAbstractBusinessAction::getBusinessRuleId() const
{
    return m_businessRuleId;
}

void QnAbstractBusinessAction::setToggleState(QnBusiness::EventState value)
{
    m_toggleState = value;
}

QnBusiness::EventState QnAbstractBusinessAction::getToggleState() const
{
    return m_toggleState;
}

void QnAbstractBusinessAction::setReceivedFromRemoteHost(bool value)
{
    m_receivedFromRemoteHost = value;
}

bool QnAbstractBusinessAction::isReceivedFromRemoteHost() const
{
    return m_receivedFromRemoteHost;
}

int QnAbstractBusinessAction::getAggregationCount() const
{
    return m_aggregationCount;
}

bool QnAbstractBusinessAction::isProlonged() const
{
    return QnBusiness::isActionProlonged(m_actionType, m_params);
}

void QnAbstractBusinessAction::setAggregationCount(int value)
{
    m_aggregationCount = value;
}

QString QnAbstractBusinessAction::getExternalUniqKey() const
{
    return lit("action_") + QString::number(static_cast<int>(m_actionType)) + L'_';
}

void QnAbstractBusinessAction::assign(const QnAbstractBusinessAction& other)
{
    (*this) = other;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (QnBusinessActionData), (ubjson)(json)(xml)(csv_record), _Fields)
