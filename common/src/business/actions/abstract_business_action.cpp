#include "abstract_business_action.h"

#include <QtCore/QCoreApplication>

#include <core/resource/resource.h>
#include <core/resource_management/resource_pool.h>

#include <business/business_strings_helper.h>
#include <utils/common/model_functions.h>

namespace QnBusiness {
    bool requiresCameraResource(ActionType actionType) {
        switch(actionType) {
        case UndefinedAction:
        case PanicRecordingAction:
        case SendMailAction:
        case DiagnosticsAction:
        case ShowPopupAction:
        case PlaySoundOnceAction:
        case PlaySoundAction:
        case SayTextAction:
            return false;

        case CameraOutputAction:
        case CameraOutputOnceAction:
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

    bool requiresUserResource(ActionType actionType) {
        switch(actionType) {
        case UndefinedAction:
        case PanicRecordingAction:
        case CameraOutputAction:
        case CameraOutputOnceAction:
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
            return false;

        case SendMailAction:
            return true;

        default:
            Q_ASSERT_X(false, Q_FUNC_INFO, "All action types must be handled.");
            return false;
        }
    }

    bool hasToggleState(ActionType actionType) {
        switch(actionType) {
        case UndefinedAction:
        case CameraOutputOnceAction:
        case SendMailAction:
        case DiagnosticsAction:
        case ShowPopupAction:
        case PlaySoundOnceAction:
        case SayTextAction:
        case ExecutePtzPresetAction:
        case ShowOnAlarmLayoutAction:
            return false;

        case CameraOutputAction:
        case CameraRecordingAction:
        case PanicRecordingAction:
        case PlaySoundAction:
        case BookmarkAction:
        case ShowTextOverlayAction:
            return true;

        default:
            Q_ASSERT_X(false, Q_FUNC_INFO, "All action types must be handled.");
            break;
        }
        return false;
    }

    bool canBeInstant(ActionType actionType) {
        switch(actionType) {
        case ExecutePtzPresetAction:
		case CameraOutputOnceAction:
        case SendMailAction:
        case DiagnosticsAction:
        case ShowPopupAction:
        case PlaySoundOnceAction:
        case SayTextAction:
        case BookmarkAction:
        case ShowTextOverlayAction:
            return true;
        default:
            return false;
        }
    }

    bool supportsDuration(ActionType actionType) {
        switch (actionType) {
        case BookmarkAction:
        case ShowTextOverlayAction:
            return true;
        default:
            return false;
        }
    }

    bool isActionProlonged(ActionType actionType, const QnBusinessActionParameters &parameters) {
        if (!hasToggleState(actionType))
            return false;

        switch (actionType) {
        case BookmarkAction:
        case ShowTextOverlayAction:
		case ShowOnAlarmLayoutAction:
            return parameters.durationMs <= 0;

        default:
            break;
        }

        return true;
    }

    QList<ActionType> allActions() {
        QList<ActionType> result;
        result
            << CameraOutputAction
            << CameraOutputOnceAction
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
            ;
        return result;
    }
}

QnAbstractBusinessAction::QnAbstractBusinessAction(const QnBusiness::ActionType actionType, const QnBusinessEventParameters& runtimeParams):
    m_actionType(actionType),
    m_toggleState(QnBusiness::UndefinedState), 
    m_receivedFromRemoteHost(false),
    m_runtimeParams(runtimeParams),
    m_aggregationCount(1)
{
}

QnAbstractBusinessAction::~QnAbstractBusinessAction()
{
}

QnBusiness::ActionType QnAbstractBusinessAction::actionType() const {
    return m_actionType; 
}

void QnAbstractBusinessAction::setResources(const QVector<QnUuid>& resources) {
    m_resources = resources;
}

const QVector<QnUuid>& QnAbstractBusinessAction::getResources() const {
    return m_resources;
}

void QnAbstractBusinessAction::setParams(const QnBusinessActionParameters& params) {
    m_params = params;
}

const QnBusinessActionParameters& QnAbstractBusinessAction::getParams() const {
    return m_params;
}

QnBusinessActionParameters& QnAbstractBusinessAction::getParams(){
    return m_params;
}

void QnAbstractBusinessAction::setRuntimeParams(const QnBusinessEventParameters& params) {
    m_runtimeParams = params;
}

const QnBusinessEventParameters& QnAbstractBusinessAction::getRuntimeParams() const {
    return m_runtimeParams;
}

void QnAbstractBusinessAction::setBusinessRuleId(const QnUuid& value) {
    m_businessRuleId = value;
}

QnUuid QnAbstractBusinessAction::getBusinessRuleId() const {
    return m_businessRuleId;
}

void QnAbstractBusinessAction::setToggleState(QnBusiness::EventState value) {
    m_toggleState = value;
}

QnBusiness::EventState QnAbstractBusinessAction::getToggleState() const {
    return m_toggleState;
}

void QnAbstractBusinessAction::setReceivedFromRemoteHost(bool value) {
    m_receivedFromRemoteHost = value;
}

bool QnAbstractBusinessAction::isReceivedFromRemoteHost() const {
    return m_receivedFromRemoteHost;
}

int QnAbstractBusinessAction::getAggregationCount() const
{
    return m_aggregationCount;
}

bool QnAbstractBusinessAction::isProlonged() const {
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

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnBusinessActionData, (ubjson)(json)(xml)(csv_record), QnBusinessActionData_Fields, (optional, true) )
