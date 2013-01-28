#ifndef __ABSTRACT_BUSINESS_ACTION_H_
#define __ABSTRACT_BUSINESS_ACTION_H_

#include <QSharedPointer>

#include "business_logic_common.h"
#include <core/resource/resource_fwd.h>
#include <utils/common/qnid.h>

namespace BusinessActionType
{
    enum Value
    {
        BA_CameraRecording,    // start camera recording
        BA_PanicRecording,     // activate panic recording mode
        // these actions can be executed from any endpoint. actually these actions call specified function at ec
        /*!
            parameters:\n
                - emailAddress (string, required)
        */
        BA_SendMail,

        BA_ShowPopup,

        //!change camera output state
        /*!
            parameters:\n
                - relayOutputID (string, required)          - id of output to trigger
                - relayAutoResetTimeout (uint, optional)    - timeout (in seconds) to reset camera state back
        */
        BA_CameraOutput,

        BA_Alert,              // write a record to the server's log
        BA_Bookmark,           // mark part of camera archive as undeleted

        // media server based actions
        BA_NotDefined,

        /**
         * Used when enumerating to build GUI lists, this and followed actions
         * should not be displayed.
         */
        BA_Count = BA_Alert

    };

    QString toString( Value val );


    bool isResourceRequired(Value val);

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
    explicit QnAbstractBusinessAction(const BusinessActionType::Value actionType, const QnBusinessParams& runtimeParams);

public:
    virtual ~QnAbstractBusinessAction();
    BusinessActionType::Value actionType() const { return m_actionType; }

    /*
    * Resource depend of action type.
    * For actions: BA_CameraOutput, BA_Bookmark, BA_CameraRecording, BA_PanicRecording resource MUST be camera
    * For actions: BA_SendMail, BA_Alert, BA_ShowPopup resource is not used
    */
    void setResources(const QnResourceList& resources);

    const QnResourceList& getResources();

    void setParams(const QnBusinessParams& params);
    const QnBusinessParams& getParams() const;

    void setRuntimeParams(const QnBusinessParams& params);
    const QnBusinessParams& getRuntimeParams() const;

    void setBusinessRuleId(const QnId& value);
    QnId getBusinessRuleId() const;

    void setToggleState(ToggleState::Value value);
    ToggleState::Value getToggleState() const;

    void setReceivedFromRemoteHost(bool value);
    bool isReceivedFromRemoteHost() const;

    void setAggregationCount(int value);
    int getAggregationCount() const;

    /** Return action unique key for external outfit (port number for output action e.t.c). Do not count resourceId 
    * This function help to share physical resources between actions
    * Do not used for instant actions
    */
    virtual QString getExternalUniqKey() const;

private:
    BusinessActionType::Value m_actionType;
    ToggleState::Value m_toggleState;
    bool m_receivedFromRemoteHost;
    QnResourceList m_resources;
    QnBusinessParams m_params;
    QnBusinessParams m_runtimeParams;
    QnId m_businessRuleId; // business rule, that generated this action
    int m_aggregationCount;
};

Q_DECLARE_METATYPE(BusinessActionType::Value)
Q_DECLARE_METATYPE(QnAbstractBusinessActionPtr)


#endif // __ABSTRACT_BUSINESS_ACTION_H_
