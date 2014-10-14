#include "abstract_business_action.h"

#include <QtCore/QCoreApplication>

#include <core/resource/resource.h>
#include <core/resource_management/resource_pool.h>

#include <business/business_strings_helper.h>

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
            return false;

        case SendMailAction:
            return true;

        default:
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
            return false;

        case CameraOutputAction:
        case CameraRecordingAction:
        case PanicRecordingAction:
        case PlaySoundAction:
        case BookmarkAction:
            return true;

        default:
            return false;
        }
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
            << SayTextAction;
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

void QnAbstractBusinessAction::setResources(const QVector<QnUuid>& resources) {
    m_resources = resources;
}

const QVector<QnUuid>& QnAbstractBusinessAction::getResources() const {
    return m_resources;
}

QnResourceList QnAbstractBusinessAction::getResourceObjects() const
{
    QnResourceList result;
    foreach(const QnUuid& id, m_resources)
    {
        QnResourcePtr res = qnResPool->getResourceById(id);
        if (res)
            result << res;
    }
    return result;
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

void QnAbstractBusinessAction::setAggregationCount(int value)
{
    m_aggregationCount = value;
}

QString QnAbstractBusinessAction::getExternalUniqKey() const
{
    return lit("action_") + QString::number(static_cast<int>(m_actionType)) + L'_';
}
