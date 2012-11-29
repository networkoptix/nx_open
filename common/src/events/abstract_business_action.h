#ifndef __ABSTRACT_BUSINESS_ACTION_H_
#define __ABSTRACT_BUSINESS_ACTION_H_

#include <QSharedPointer>
#include "business_logic_common.h"
#include "core/resource/resource_fwd.h"
#include "utils/common/qnid.h"

enum BusinessActionType
{
    // media server based actions
    BA_NotDefined,
    BA_CameraOutput,       // set camera output signal
    BA_Bookmark,           // mark part of camera archive as undeleted
    BA_CameraRecording,    // start camera recording
    BA_PanicRecording,     // activate panic recording mode
    //!change camera output state
    /*!
        Parameters:\n
            - relayOutputID (string, required)          - id of output to trigger
            - relayAutoResetTimeout (uint, optional)    - timeout (in seconds) to reset camera state back
    */
    BA_TriggerOutput,

    // these actions can be executed from any endpoint. Actually these actions call specified function at EC
    /*!
        Parameters:\n
            - emailAddress (string, required)
    */
    BA_SendMail,
    BA_Alert,
    BA_ShowPopup
};

namespace BusinessActionParamName
{
    static QLatin1String relayOutputID( "relayOutputID" );
    static QLatin1String relayAutoResetTimeout( "relayAutoResetTimeout" );
    static QLatin1String emailAddress( "emailAddress" );
}

class QnAbstractBusinessAction;
typedef QSharedPointer<QnAbstractBusinessAction> QnAbstractBusinessActionPtr;

/*
* Base class for business actions
*/

class QnAbstractBusinessAction
{
public:
    QnAbstractBusinessAction();
    virtual ~QnAbstractBusinessAction() {}
    BusinessActionType actionType() const { return m_actionType; }

    QByteArray serialize();
    static QnAbstractBusinessActionPtr fromByteArray(const QByteArray&);

    /*
    * Resource depend of action type.
    * For actions: BA_CameraOutput, BA_Bookmark, BA_CameraRecording, BA_PanicRecording resource MUST be camera
    * For actions: BA_SendMail, BA_Alert resource is not used
    */
    void setResource(QnResourcePtr resource)   { m_resource = resource; }

    const QnResourcePtr& getResource()                { return m_resource;     }

    void setParams(const QnBusinessParams& params) {m_params = params; }
    const QnBusinessParams& getParams() const             { return m_params; }

    void setBusinessRuleId(const QnId& value) {m_businessRuleId = value; }
    QnId getBusinessRuleId() const             { return m_businessRuleId; }

    void setToggleState(ToggleState value) { m_toggleState = value; }
    ToggleState getToggleState() const { return m_toggleState; }

    void setReceivedFromRemoveHost(bool value) { m_receivedFromRemoveHost = true; }
    bool isReceivedFromRemoveHost() const { return m_receivedFromRemoveHost; }

    bool isToggledAction() const;
protected:
    void setActionType(BusinessActionType value) { m_actionType = value; }

    BusinessActionType m_actionType;
    QnResourcePtr m_resource;
    QnBusinessParams m_params;
    QnId m_businessRuleId; // business rule, that generated this action

private:
    ToggleState m_toggleState;
    bool m_receivedFromRemoveHost;
};

Q_DECLARE_METATYPE(QnAbstractBusinessActionPtr)


#endif // __ABSTRACT_BUSINESS_ACTION_H_
