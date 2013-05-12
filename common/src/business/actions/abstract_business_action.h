#ifndef __ABSTRACT_BUSINESS_ACTION_H_
#define __ABSTRACT_BUSINESS_ACTION_H_

#include <QSharedPointer>

#include <business/business_logic_common.h>
#include <core/resource/resource_fwd.h>
#include <utils/common/qnid.h>

#include "../business_action_parameters.h"
#include "business/business_event_parameters.h"

namespace BusinessActionType
{

    enum Value
    {
        CameraRecording,    // start camera recording
        PanicRecording,     // activate panic recording mode
        // these actions can be executed from any endpoint. actually these actions call specified function at ec
        /*!
            parameters:\n
                - emailAddress (string, required)
        */
        SendMail,

        ShowPopup,

        //!change camera output state
        /*!
            parameters:\n
                - relayOutputID (string, required)          - id of output to trigger
                - relayAutoResetTimeout (uint, optional)    - timeout (in milliseconds) to reset camera state back
        */
        CameraOutput,
        CameraOutputInstant,

        /*!
            parameters:\n
                - soundSource (int, required)               - enumeration describing source of the sound (resources, EC, TTS)
                - soundUrl (string, required)               - url of sound, can contain:
                                                                * path to sound on the EC
                                                                * path to the resource
                                                                * text that will be provided to TTS engine
        */
        PlaySound,

        Alert,              // write a record to the server's log
        Bookmark,           // mark part of camera archive as undeleted

        // media server based actions
        NotDefined,

        /**
         * Used when enumerating to build GUI lists, this and followed actions
         * should not be displayed.
         */
        Count = Alert

    };

    QString toString( Value val );

    //TODO: #GDM fix to resourceTypeRequired: None, Camera, Server, User, etc
    bool requiresCameraResource(Value val);
    bool requiresUserResource(Value val);

    bool hasToggleState(Value val);
}

class QnAbstractBusinessAction;
typedef QSharedPointer<QnAbstractBusinessAction> QnAbstractBusinessActionPtr;



/*
* Base class for business actions
*/

class QnAbstractBusinessAction
{
protected:

    explicit QnAbstractBusinessAction(const BusinessActionType::Value actionType, const QnBusinessEventParameters& runtimeParams);

public:
    virtual ~QnAbstractBusinessAction();
    BusinessActionType::Value actionType() const { return m_actionType; }

    /*
    * Resource depend of action type.
    * see: BusinessActionType::requiresCameraResource()
    * see: BusinessActionType::requiresUserResource()
    */
    void setResources(const QnResourceList& resources);

    const QnResourceList& getResources() const;

    void setParams(const QnBusinessActionParameters& params);
    const QnBusinessActionParameters& getParams() const;

    void setRuntimeParams(const QnBusinessEventParameters& params);
    const QnBusinessEventParameters& getRuntimeParams() const;

    void setBusinessRuleId(const QnId& value);
    QnId getBusinessRuleId() const;

    void setToggleState(ToggleState::Value value);
    ToggleState::Value getToggleState() const;

    void setReceivedFromRemoteHost(bool value);
    bool isReceivedFromRemoteHost() const;

    /** How much times action was instantiated during the aggregation period. */
    void setAggregationCount(int value);
    int getAggregationCount() const;

    /** Return action unique key for external outfit (port number for output action e.t.c). Do not count resourceId 
    * This function help to share physical resources between actions
    * Do not used for instant actions
    */
    virtual QString getExternalUniqKey() const;

protected:
    BusinessActionType::Value m_actionType;
    ToggleState::Value m_toggleState;
    bool m_receivedFromRemoteHost;
    QnResourceList m_resources;
    QnBusinessActionParameters m_params;
    QnBusinessEventParameters m_runtimeParams;
    QnId m_businessRuleId; // business rule that generated this action
    int m_aggregationCount;
};

typedef QList<QnAbstractBusinessActionPtr> QnAbstractBusinessActionList;

class QnLightBusinessAction
{
public:
    QnLightBusinessAction(): m_actionType(BusinessActionType::NotDefined) {}

    virtual ~QnLightBusinessAction() {}
    
    BusinessActionType::Value actionType() const { return m_actionType; }
    void setActionType(BusinessActionType::Value type) { m_actionType = type; }

    //void setParams(const QnBusinessActionParameters& params) { m_params = params;}
    //const QnBusinessActionParameters& getParams() const { return m_params; }

    void setRuntimeParams(const QnBusinessEventParameters& params) {m_runtimeParams = params;}
    const QnBusinessEventParameters& getRuntimeParams() const {return m_runtimeParams; }

    void setBusinessRuleId(const QnId& value) {m_businessRuleId = value; }
    QnId getBusinessRuleId() const { return m_businessRuleId; }

    void setAggregationCount(int value) { m_aggregationCount = value; }
    int getAggregationCount() const { return m_aggregationCount; }
protected:
    BusinessActionType::Value m_actionType;
    //QnBusinessActionParameters m_params;
    QnBusinessEventParameters m_runtimeParams;
    QnId m_businessRuleId; 
    int m_aggregationCount;
};

typedef std::vector<QnLightBusinessAction> QnLightBusinessActionVector;


Q_DECLARE_METATYPE(BusinessActionType::Value)
Q_DECLARE_METATYPE(QnAbstractBusinessActionPtr)
Q_DECLARE_METATYPE(QnAbstractBusinessActionList)
Q_DECLARE_METATYPE(QnLightBusinessActionVector)


#endif // __ABSTRACT_BUSINESS_ACTION_H_
