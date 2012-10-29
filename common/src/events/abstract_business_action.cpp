#include "abstract_business_action.h"

QByteArray QnAbstractBusinessAction::serialize()
{
    pb::BusinessAction pb_businessAction;

    pb_businessAction.set_actiontype(actionType());
    pb_businessAction.set_actionresource(getResource()->getId());
    pb_businessAction.set_actionParams(serializeBusinessParams(getParams()));
    pb_businessAction.set_businessRuleId(getBusinessRuleId().toInt());
}

static QnAbstractBusinessActionPtr QnAbstractBusinessAction::fromByteArray(const QByteArray& data)
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
        case pb::BusinessAction::CameraOutput:
        case pb::BusinessAction::Bookmark:
        case pb::BusinessAction::CameraRecording:
        case pb::BusinessAction::PanicRecording:
        case pb::BusinessAction::SendMail:
        case pb::BusinessAction::Alert:
        case pb::BusinessActio::ShowPopup:
            businessAction = QnAbstractBusinessActionPtr(new QnBusinessEventRule());
    }

    businessAction->setActionType(pb_businessAction.actiontype());
    businessAction->setResource(pb_businessAction.actionresource());
    businessAction->setParams(deserializeBusinessParams(ci->actionparams.c_str()));
    businessAction->setBusinessRuleId(pb_businessAction.businessruleid());

    return businessAction;
}
