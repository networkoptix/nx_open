#include "abstract_business_action.h"

#include <QtCore/QCoreApplication>

#include <core/resource/resource.h>
#include <core/resource_management/resource_pool.h>

#include <business/business_strings_helper.h>

namespace BusinessActionType {
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

        default:
            return false;
        }
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

        default:
            return false;
        }
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

        default:
            return false;
        }
    }

    bool isNotImplemented(Value value) { return value == Bookmark; }
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
    return lit("action_") + QString::number(static_cast<int>(m_actionType)) + L'_';
}
