#include "core/resource/resource.h"
#include "core/resource_managment/resource_pool.h"

#include "businessRule.pb.h"

#include "api/serializer/serializer.h"
#include "abstract_business_action.h"
#include <core/resource/resource.h>

namespace BusinessActionType
{
    QString toString( Value val )
    {
        switch( val )
        {
            case BA_NotDefined:
                return QObject::tr("---");
            case BA_CameraOutput:
                return QObject::tr("Camera output");
            case BA_Bookmark:
                return QObject::tr("Bookmark");
            case BA_CameraRecording:
                return QObject::tr("Camera recording");
            case BA_PanicRecording:
                return QObject::tr("Panic recording");
            case BA_SendMail:
                return QObject::tr("Send mail");
            case BA_Alert:
                return QObject::tr("Alert");
            case BA_ShowPopup:
                return QObject::tr("Show popup");
            //warning should be raised on unknown enumeration values
        }

        return QObject::tr("Unknown (%1)").arg((int)val);
    }

    bool isResourceRequired(Value val) {
        switch( val )
        {
            case BA_NotDefined:
                return false;
            case BA_CameraOutput:
                return true;
            case BA_Bookmark:
                return true;
            case BA_CameraRecording:
                return true;
            case BA_PanicRecording:
                return false;
            case BA_SendMail:
                return false;
            case BA_Alert:
                return false;
            case BA_ShowPopup:
                return false;
            //warning should be raised on unknown enumeration values
        }
        return false;
    }

    bool hasToggleState(Value val) {
        switch( val )
        {
            case BA_NotDefined:
                return false;
            case BA_CameraOutput:
                return true;
            case BA_Bookmark:
                return false;
            case BA_CameraRecording:
                return true;
            case BA_PanicRecording:
                return true;
            case BA_SendMail:
                return false;
            case BA_Alert:
                return false;
            case BA_ShowPopup:
                return false;
            //warning should be raised on unknown enumeration values
        }

        return false;
    }
}

QnAbstractBusinessAction::QnAbstractBusinessAction(BusinessActionType::Value actionType):
    m_actionType(actionType),
    m_toggleState(ToggleState::NotDefined), 
    m_receivedFromRemoteHost(false)
{

}

QByteArray QnAbstractBusinessAction::serialize()
{
    pb::BusinessAction pb_businessAction;

    pb_businessAction.set_actiontype((pb::BusinessActionType)actionType());
    if( getResource() )
        pb_businessAction.set_actionresource(getResource()->getId());
    pb_businessAction.set_actionparams(serializeBusinessParams(getParams()));
    pb_businessAction.set_businessruleid(getBusinessRuleId().toInt());

    std::string str;
    pb_businessAction.SerializeToString(&str);
    return QByteArray(str.data(), str.length());
}

QnAbstractBusinessActionPtr QnAbstractBusinessAction::fromByteArray(const QByteArray& data)
{
    pb::BusinessAction pb_businessAction;

    if (!pb_businessAction.ParseFromArray(data.data(), data.size()))
    {
        QByteArray errorString;
        errorString = "QnAbstractBusinessAction::fromByteArray(): Can't parse message";
        throw QnSerializeException(errorString);
    }

    QnAbstractBusinessActionPtr businessAction;

    switch(pb_businessAction.actiontype())
    {
        case pb::CameraOutput:
        case pb::Bookmark:
        case pb::CameraRecording:
        case pb::PanicRecording:
        case pb::SendMail:
        case pb::Alert:
        case pb::ShowPopup:
            businessAction = QnAbstractBusinessActionPtr(new QnAbstractBusinessAction((BusinessActionType::Value)pb_businessAction.actiontype()));
    }

    businessAction->setResource(qnResPool->getResourceById(pb_businessAction.actionresource()));
    businessAction->setParams(deserializeBusinessParams(pb_businessAction.actionparams().c_str()));
    businessAction->setBusinessRuleId(pb_businessAction.businessruleid());

    return businessAction;
}

void QnAbstractBusinessAction::setResource(QnResourcePtr resource) {
    m_resource = resource;
}

const QnResourcePtr& QnAbstractBusinessAction::getResource() {
    return m_resource;
}

void QnAbstractBusinessAction::setParams(const QnBusinessParams& params) {
    m_params = params;
}

const QnBusinessParams& QnAbstractBusinessAction::getParams() const {
    return m_params;
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
