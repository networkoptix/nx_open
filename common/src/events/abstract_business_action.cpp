#include "core/resource/resource.h"
#include "core/resource_managment/resource_pool.h"

#include "businessRule.pb.h"

#include "api/serializer/serializer.h"
#include "abstract_business_action.h"


namespace BusinessActionType
{
    QString toString( Value val )
    {
        switch( val )
        {
            case BA_NotDefined:
                return QLatin1String("Not defined");
            case BA_CameraOutput:
                return QLatin1String("Camera output");
            case BA_Bookmark:
                return QLatin1String("Bookmark");
            case BA_CameraRecording:
                return QLatin1String("Camera recording");
            case BA_PanicRecording:
                return QLatin1String("Panic recording");
            case BA_TriggerOutput:
                return QLatin1String("Trigger output");
            case BA_SendMail:
                return QLatin1String("Send mail");
            case BA_Alert:
                return QLatin1String("Alert");
            case BA_ShowPopup:
                return QLatin1String("Show popup");
            default:
                return QString::fromLatin1("unknown (%1)").arg((int)val);
        }
    }
}


QnAbstractBusinessAction::QnAbstractBusinessAction(): 
    m_actionType(BusinessActionType::BA_NotDefined),
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

QnAbstractBusinessActionPtr QnAbstractBusinessAction::fromByteArray2(const QByteArray& data)
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
            businessAction = QnAbstractBusinessActionPtr(new QnAbstractBusinessAction());
    }

    businessAction->setActionType((BusinessActionType::Value)pb_businessAction.actiontype());
    businessAction->setResource(qnResPool->getResourceById(pb_businessAction.actionresource()));
    businessAction->setParams(deserializeBusinessParams(pb_businessAction.actionparams().c_str()));
    businessAction->setBusinessRuleId(pb_businessAction.businessruleid());

    return businessAction;
}

bool QnAbstractBusinessAction::isToggledAction() const
{
    switch(m_actionType)
    {
        case BusinessActionType::BA_CameraOutput:
        case BusinessActionType::BA_CameraRecording:
        case BusinessActionType::BA_PanicRecording:
            return true;
        default:
            return false;
    }
}
