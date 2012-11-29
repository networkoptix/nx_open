#include "core/resource/resource.h"
#include "core/resource_managment/resource_pool.h"

#include "businessRule.pb.h"

#include "api/serializer/serializer.h"
#include "abstract_business_action.h"

QnAbstractBusinessAction::QnAbstractBusinessAction(): 
    m_actionType(BA_NotDefined),
    m_toggleState(ToggleState_NotDefined), 
    m_receivedFromRemoveHost(false) 
{

}

QByteArray QnAbstractBusinessAction::serialize()
{
    pb::BusinessAction pb_businessAction;

    pb_businessAction.set_actiontype((pb::BusinessActionType)actionType());
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
        case pb::BusinessActionType::CameraOutput:
        case pb::BusinessActionType::Bookmark:
        case pb::BusinessActionType::CameraRecording:
        case pb::BusinessActionType::PanicRecording:
        case pb::BusinessActionType::SendMail:
        case pb::BusinessActionType::Alert:
        case pb::BusinessActionType::ShowPopup:
            businessAction = QnAbstractBusinessActionPtr(new QnAbstractBusinessAction());
    }

    businessAction->setActionType((BusinessActionType)pb_businessAction.actiontype());
    businessAction->setResource(qnResPool->getResourceById(pb_businessAction.actionresource()));
    businessAction->setParams(deserializeBusinessParams(pb_businessAction.actionparams().c_str()));
    businessAction->setBusinessRuleId(pb_businessAction.businessruleid());

    return businessAction;
}

bool QnAbstractBusinessAction::isToggledAction() const
{
    switch(m_actionType)
    {
        case BA_CameraOutput:
        case BA_CameraRecording:
        case BA_PanicRecording:
            return true;
        default:
            return false;
    }
}
