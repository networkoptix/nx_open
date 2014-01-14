#include "abstract_business_action.h"

#include <core/resource/resource.h>
#include <core/resource_managment/resource_pool.h>

namespace BusinessActionType {

    //do not use 'default' keyword: warning should be raised on unknown enumeration values

    QString toString(Value val) {
        switch(val) {
        case NotDefined:            return QString();
        case CameraOutput:          return QObject::tr("Camera output");
        case CameraOutputInstant:   return QObject::tr("Camera output for 30 sec");
        case Bookmark:              return QObject::tr("Bookmark");
        case CameraRecording:       return QObject::tr("Camera recording");
        case PanicRecording:        return QObject::tr("Panic recording");
        case SendMail:              return QObject::tr("Send mail");
        case Diagnostics:           return QObject::tr("Write to log");
        case ShowPopup:             return QObject::tr("Show notification");
        case PlaySound:             return QObject::tr("Play sound");
        case PlaySoundRepeated:     return QObject::tr("Repeat sound");
        case SayText:               return QObject::tr("Speak");
        }
        return QObject::tr("Unknown (%1)").arg((int)val);
    }

    bool requiresCameraResource(Value val) {
        switch(val) {
        case NotDefined:
        case PanicRecording:
        case SendMail:
        case Diagnostics:
        case ShowPopup:
        case PlaySound:
        case PlaySoundRepeated:
        case SayText:
            return false;

        case CameraOutput:
        case CameraOutputInstant:
        case Bookmark:
        case CameraRecording:
            return true;
        }
        return false;
    }

    bool requiresUserResource(Value val) {
        switch(val) {
        case NotDefined:
        case PanicRecording:
        case CameraOutput:
        case CameraOutputInstant:
        case Bookmark:
        case CameraRecording:
        case Diagnostics:
        case ShowPopup:
        case PlaySound:
        case PlaySoundRepeated:
        case SayText:
            return false;

        case SendMail:
            return true;
        }
        return false;
    }

    bool hasToggleState(Value val) {
        switch(val) {
        case NotDefined:
        case CameraOutputInstant:
        case Bookmark:
        case SendMail:
        case Diagnostics:
        case ShowPopup:
        case PlaySound:
        case SayText:
            return false;

        case CameraOutput:
        case CameraRecording:
        case PanicRecording:
        case PlaySoundRepeated:
            return true;
        }
        return false;
    }
}

QnAbstractBusinessAction::QnAbstractBusinessAction(const BusinessActionType::Value actionType, const QnBusinessEventParameters& runtimeParams):
    m_actionType(actionType),
    m_toggleState(Qn::UndefinedState), 
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

void QnAbstractBusinessAction::setBusinessRuleId(const QnId& value) {
    m_businessRuleId = value;
}

QnId QnAbstractBusinessAction::getBusinessRuleId() const {
    return m_businessRuleId;
}

void QnAbstractBusinessAction::setToggleState(Qn::ToggleState value) {
    m_toggleState = value;
}

Qn::ToggleState QnAbstractBusinessAction::getToggleState() const {
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
