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
        //!change camera output state
        /*!
            parameters:\n
                - relayOutputID (string, required)          - id of output to trigger
                - relayAutoResetTimeout (uint, optional)    - timeout (in seconds) to reset camera state back
        */
        BA_CameraOutput,
        BA_Bookmark,           // mark part of camera archive as undeleted
        BA_CameraRecording,    // start camera recording
        BA_PanicRecording,     // activate panic recording mode
        // these actions can be executed from any endpoint. actually these actions call specified function at ec
        /*!
            parameters:\n
                - emailAddress (string, required)
        */
        BA_SendMail,
        BA_Alert,
        BA_ShowPopup,
        // media server based actions
        BA_NotDefined,

        BA_FirstType = BA_CameraOutput,
        BA_LastType = BA_ShowPopup
    };

    QString toString( Value val );

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
    explicit QnAbstractBusinessAction(BusinessActionType::Value actionType);

public:
    virtual ~QnAbstractBusinessAction() {}
    BusinessActionType::Value actionType() const { return m_actionType; }

    QByteArray serialize();
    static QnAbstractBusinessActionPtr fromByteArray(const QByteArray&);

    /*
    * Resource depend of action type.
    * For actions: BA_CameraOutput, BA_Bookmark, BA_CameraRecording, BA_PanicRecording resource MUST be camera
    * For actions: BA_SendMail, BA_Alert resource is not used
    */
    void setResource(QnResourcePtr resource);

    const QnResourcePtr& getResource();

    void setParams(const QnBusinessParams& params);
    const QnBusinessParams& getParams() const;

    void setBusinessRuleId(const QnId& value);
    QnId getBusinessRuleId() const;

    void setToggleState(ToggleState::Value value);
    ToggleState::Value getToggleState() const;

    void setReceivedFromRemoteHost(bool value);
    bool isReceivedFromRemoteHost() const;
private:
    BusinessActionType::Value m_actionType;
    ToggleState::Value m_toggleState;
    bool m_receivedFromRemoteHost;
    QnResourcePtr m_resource;
    QnBusinessParams m_params;
    QnId m_businessRuleId; // business rule, that generated this action
};

Q_DECLARE_METATYPE(QnAbstractBusinessActionPtr)


#endif // __ABSTRACT_BUSINESS_ACTION_H_
