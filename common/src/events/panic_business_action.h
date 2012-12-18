/**********************************************************
* 29 nov 2012
* a.kolesnikov
***********************************************************/

#ifndef PANICBUSINESSACTION_H
#define PANICMAILBUSINESSACTION_H

#include "abstract_business_action.h"
#include "abstract_business_event.h"

#include <core/resource/resource_fwd.h>

namespace BusinessActionParameters {
    QString getEmailAddress(const QnBusinessParams &params);
    void setEmailAddress(QnBusinessParams* params, const QString &value);
}

class QnPanicBusinessAction: public QnAbstractBusinessAction
{
    typedef QnAbstractBusinessAction base_type;
public:
    QnPanicBusinessAction( BusinessEventType::Value eventType, QnResourcePtr resource, QString eventDescription );

private:
};

typedef QSharedPointer<QnPanicBusinessAction> QnPanicBusinessActionPtr;

#endif  //PANICBUSINESSACTION_H
