#include "abstract_business_action.h"

#include <QtCore/QCoreApplication>

#include <core/resource/resource.h>
#include <core/resource_managment/resource_pool.h>

// TODO: #Elric move to BusinessStringsHelper?
class QnBusinessActionStrings {
    Q_DECLARE_TR_FUNCTIONS(QnBusinessActionStrings)
public:
    static QString toString(BusinessActionType::Value type) {
        //do not use 'default' keyword: warning should be raised on unknown enumeration values
        using namespace BusinessActionType;
        switch(type) {
        case NotDefined:            return QString();
        case CameraOutput:          return tr("Camera output");
        case CameraOutputInstant:   return tr("Camera output for 30 sec");
        case Bookmark:              return tr("Bookmark");
        case CameraRecording:       return tr("Camera recording");
        case PanicRecording:        return tr("Panic recording");
        case SendMail:              return tr("Send mail");
        case Diagnostics:           return tr("Write to log");
        case ShowPopup:             return tr("Show notification");
        case PlaySound:             return tr("Play sound");
        case PlaySoundRepeated:     return tr("Repeat sound");
        case SayText:               return tr("Say");
        }
        return tr("Unknown (%1)").arg(static_cast<int>(type));
    }
};

namespace BusinessActionType {

    QString toString(Value type) {
        return QnBusinessActionStrings::toString(type);
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
