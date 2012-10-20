#ifndef __ABSTRACT_BUSINESS_ACTION_H_
#define __ABSTRACT_BUSINESS_ACTION_H_

#include <QSharedPointer>
#include "abstract_business_event.h"
#include "core/resource/resource_fwd.h"

enum BusinessActionType
{
    BA_CameraOutput,
    BA_SendMail,
    BA_Bookmark,
    BA_Alert,
    BA_CameraRecording,
    BA_PanicRecording
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
    * Execute action. Return true if execute success
    */
    virtual bool execute() = 0;

    void setResource(QnResourcePtr resource)   { m_resource = resource; }
    QnResourcePtr getResource()                { return m_resource;     }
protected:
    void setActionType(BusinessActionType value) { m_actionType = value; }
private:
    BusinessActionType m_actionType;
    QnResourcePtr m_resource;
};

typedef QSharedPointer<QnAbstractBusinessAction> QnAbstractBusinessActionPtr;

#endif // __ABSTRACT_BUSINESS_ACTION_H_
