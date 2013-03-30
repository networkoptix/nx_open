#include "abstract_business_action.h"

#include <core/resource/resource.h>
#include <core/resource_managment/resource_pool.h>

namespace BusinessActionType {

    //do not use 'default' keyword: warning should be raised on unknown enumeration values

    QString toString(Value val) {
        switch(val) {
            case NotDefined:         return QObject::tr("---");
            case CameraOutput:       return QObject::tr("Camera output");
            case Bookmark:           return QObject::tr("Bookmark");
            case CameraRecording:    return QObject::tr("Camera recording");
            case PanicRecording:     return QObject::tr("Panic recording");
            case SendMail:           return QObject::tr("Send mail");
            case Alert:              return QObject::tr("Alert");
            case ShowPopup:          return QObject::tr("Show notification");
        }
        return QObject::tr("Unknown (%1)").arg((int)val);
    }

    bool requiresCameraResource(Value val) {
        switch(val) {
            case NotDefined:         return false;
            case CameraOutput:       return true;
            case Bookmark:           return true;
            case CameraRecording:    return true;
            case PanicRecording:     return false;
            case SendMail:           return false;
            case Alert:              return false;
            case ShowPopup:          return false;
        }
        return false;
    }

    bool requiresUserResource(Value val) {
        switch(val) {
            case SendMail:           return true;
        default:
            break;
        }
        return false;
    }

    bool hasToggleState(Value val) {
        switch(val) {
            case NotDefined:         return false;
            case CameraOutput:       return true;
            case Bookmark:           return false;
            case CameraRecording:    return true;
            case PanicRecording:     return true;
            case SendMail:           return false;
            case Alert:              return false;
            case ShowPopup:          return false;
        }
        return false;
    }
}

QnAbstractBusinessAction::QnAbstractBusinessAction(const BusinessActionType::Value actionType, const QnBusinessParams& runtimeParams):
    m_actionType(actionType),
    m_toggleState(ToggleState::NotDefined), 
    m_receivedFromRemoteHost(false),
    m_runtimeParams(runtimeParams),
    m_aggregationCount(1)
{
}

QnAbstractBusinessAction::~QnAbstractBusinessAction()
{
}

void QnAbstractBusinessAction::setResources(const QnResourceList& resources) {
    m_resources = resources;
}

const QnResourceList& QnAbstractBusinessAction::getResources() const {
    return m_resources;
}

void QnAbstractBusinessAction::setParams(const QnBusinessParams& params) {
    m_params = params;
}

const QnBusinessParams& QnAbstractBusinessAction::getParams() const {
    return m_params;
}

void QnAbstractBusinessAction::setRuntimeParams(const QnBusinessParams& params) {
    m_runtimeParams = params;
}

const QnBusinessParams& QnAbstractBusinessAction::getRuntimeParams() const {
    return m_runtimeParams;
}

void QnAbstractBusinessAction::setBusinessRuleId(const QnId& value) {
    m_businessRuleId = value;
}

QnId QnAbstractBusinessAction::getBusinessRuleId() const {
    return m_businessRuleId;
}

void QnAbstractBusinessAction::setToggleState(ToggleState::Value value) {
    m_toggleState = value;
}

ToggleState::Value QnAbstractBusinessAction::getToggleState() const {
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
    return BusinessActionType::toString(m_actionType);
}
