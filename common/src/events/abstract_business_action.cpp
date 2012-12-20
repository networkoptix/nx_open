#include "core/resource/resource.h"
#include "core/resource_managment/resource_pool.h"

#include "businessRule.pb.h"

#include "api/serializer/serializer.h"
#include "abstract_business_action.h"
#include "business_action_factory.h"
#include <core/resource/resource.h>

namespace BusinessActionType {

    //do not use 'default' keyword: warning should be raised on unknown enumeration values

    QString toString(Value val) {
        switch(val) {
            case BA_NotDefined:         return QObject::tr("---");
            case BA_CameraOutput:       return QObject::tr("Camera output");
            case BA_Bookmark:           return QObject::tr("Bookmark");
            case BA_CameraRecording:    return QObject::tr("Camera recording");
            case BA_PanicRecording:     return QObject::tr("Panic recording");
            case BA_SendMail:           return QObject::tr("Send mail");
            case BA_Alert:              return QObject::tr("Alert");
            case BA_ShowPopup:          return QObject::tr("Show popup");
        }
        return QObject::tr("Unknown (%1)").arg((int)val);
    }

    bool isResourceRequired(Value val) {
        switch(val) {
            case BA_NotDefined:         return false;
            case BA_CameraOutput:       return true;
            case BA_Bookmark:           return true;
            case BA_CameraRecording:    return true;
            case BA_PanicRecording:     return false;
            case BA_SendMail:           return false;
            case BA_Alert:              return false;
            case BA_ShowPopup:          return false;
        }
        return false;
    }

    bool hasToggleState(Value val) {
        switch(val) {
            case BA_NotDefined:         return false;
            case BA_CameraOutput:       return true;
            case BA_Bookmark:           return false;
            case BA_CameraRecording:    return true;
            case BA_PanicRecording:     return true;
            case BA_SendMail:           return false;
            case BA_Alert:              return false;
            case BA_ShowPopup:          return false;
        }
        return false;
    }

    pb::BusinessActionType toProtobuf(Value val) {
        switch(val) {
            case BA_NotDefined:         return pb::Alert;
            case BA_CameraOutput:       return pb::CameraOutput;
            case BA_Bookmark:           return pb::Bookmark;
            case BA_CameraRecording:    return pb::CameraRecording;
            case BA_PanicRecording:     return pb::PanicRecording;
            case BA_SendMail:           return pb::SendMail;
            case BA_Alert:              return pb::Alert;
            case BA_ShowPopup:          return pb::ShowPopup;
        }
        return pb::Alert;        //TODO: think about adding undefined action to protobuf
    }

    Value fromProtobuf(pb::BusinessActionType actionType) {
        switch (actionType) {
            case pb::CameraOutput:      return BA_CameraOutput;
            case pb::Bookmark:          return BA_Bookmark;
            case pb::CameraRecording:   return BA_CameraRecording;
            case pb::PanicRecording:    return BA_PanicRecording;
            case pb::SendMail:          return BA_SendMail;
            case pb::Alert:             return BA_Alert;
            case pb::ShowPopup:         return BA_ShowPopup;
        }
        return BA_NotDefined;
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

    pb_businessAction.set_actiontype(BusinessActionType::toProtobuf(m_actionType));
    if( getResource() )
        pb_businessAction.set_actionresource(getResource()->getId());
    pb_businessAction.set_actionparams(serializeBusinessParams(getParams()));
    pb_businessAction.set_businessruleid(getBusinessRuleId().toInt());
    pb_businessAction.set_togglestate((pb::ToggleStateType) getToggleState());

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

    QnBusinessParams runtimeParams;
    if (pb_businessAction.has_runtimeparams())
        runtimeParams = deserializeBusinessParams(pb_businessAction.runtimeparams().c_str());
    QnAbstractBusinessActionPtr businessAction = QnBusinessActionFactory::createAction(
                BusinessActionType::fromProtobuf(pb_businessAction.actiontype()),
                runtimeParams);

    businessAction->setResource(qnResPool->getResourceById(pb_businessAction.actionresource()));
    businessAction->setParams(deserializeBusinessParams(pb_businessAction.actionparams().c_str()));
    businessAction->setBusinessRuleId(pb_businessAction.businessruleid());
    businessAction->setToggleState((ToggleState::Value) pb_businessAction.togglestate());

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
