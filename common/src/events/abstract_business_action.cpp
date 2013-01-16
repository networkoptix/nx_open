#include "core/resource/resource.h"
#include "core/resource_managment/resource_pool.h"

#include "businessRule.pb.h"

#include "api/serializer/serializer.h"
#include "api/serializer/pb_serializer.h"
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

QByteArray QnAbstractBusinessAction::serialize()
{
    pb::BusinessAction pb_businessAction;

    pb_businessAction.set_actiontype((pb::BusinessActionType) serializeBusinessActionTypeToPb(m_actionType));
    foreach(QnResourcePtr res, getResources())
        pb_businessAction.add_actionresource(res->getId());
    pb_businessAction.set_actionparams(serializeBusinessParams(getParams()));
    pb_businessAction.set_runtimeparams(serializeBusinessParams(getRuntimeParams()));
    pb_businessAction.set_businessruleid(getBusinessRuleId().toInt());
    pb_businessAction.set_togglestate((pb::ToggleStateType) getToggleState());
    pb_businessAction.set_aggregationcount(getAggregationCount());

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
                parsePbBusinessActionType(pb_businessAction.actiontype()),
                runtimeParams);

    QnResourceList resources;
    for (int i = 0; i < pb_businessAction.actionresource_size(); i++) //destination resource can belong to another server
        resources << qnResPool->getResourceById(pb_businessAction.actionresource(i), QnResourcePool::rfAllResources);
    businessAction->setResources(resources);

    businessAction->setParams(deserializeBusinessParams(pb_businessAction.actionparams().c_str()));
    businessAction->setBusinessRuleId(pb_businessAction.businessruleid());
    businessAction->setToggleState((ToggleState::Value) pb_businessAction.togglestate());
    businessAction->setAggregationCount(pb_businessAction.aggregationcount());

    return businessAction;
}

void QnAbstractBusinessAction::setResources(const QnResourceList& resources) {
    m_resources = resources;
}

const QnResourceList& QnAbstractBusinessAction::getResources() {
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
