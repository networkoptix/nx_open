#ifndef __ABSTRACT_BUSINESS_ACTION_H_
#define __ABSTRACT_BUSINESS_ACTION_H_

#include <QSharedPointer>
#include "business_logic_common.h"
#include "core/resource/resource_fwd.h"
#include "utils/common/qnid.h"

enum BusinessActionType
{
    // media server based actions
    BA_CameraOutput,       // set camera output signal
    BA_Bookmark,           // mark part of camera archive as undeleted
    BA_CameraRecording,    // start camera recording
    BA_PanicRecording,     // activate panic recording mode

    // these actions can be executed from any endpoint. Actually these actions call specified function at EC
    BA_SendMail,
    BA_Alert,
    BA_ShowPopup
};

/*
* Base class for business actions
*/

class QnAbstractBusinessAction
{
public:
    QnAbstractBusinessAction() {}
    virtual ~QnAbstractBusinessAction() {}
    BusinessActionType actionType() const { return m_actionType; }

    virtual QByteArray serialize() = 0;
    virtual bool deserialize(const QByteArray& data) = 0;

    /*
    * Resource depend of action type.
    * For actions: BA_CameraOutput, BA_Bookmark, BA_CameraRecording, BA_PanicRecording resource MUST be camera
    * For actions: BA_SendMail, BA_Alert resource is not used
    */
    void setResource(QnResourcePtr resource)   { m_resource = resource; }

    QnResourcePtr getResource()                { return m_resource;     }

    void setParams(const QnBusinessParams& params) {m_params = params; }
    QnBusinessParams getParams() const             { return m_params; }

    void setBusinessRuleId(const QnId& value) {m_businessRuleId = value; }
    QnId getBusinessRuleId() const             { return m_businessRuleId; }

    QByteArray serializeParams() const;
    bool deserializeParams(const QByteArray& value);
protected:
    void setActionType(BusinessActionType value) { m_actionType = value; }

    BusinessActionType m_actionType;
    QnResourcePtr m_resource;
    QnBusinessParams m_params;
    QnId m_businessRuleId; // business rule, that generated this action
};

typedef QSharedPointer<QnAbstractBusinessAction> QnAbstractBusinessActionPtr;
Q_DECLARE_METATYPE(QnAbstractBusinessActionPtr)


#endif // __ABSTRACT_BUSINESS_ACTION_H_
