/**********************************************************
* 29 nov 2012
* a.kolesnikov
***********************************************************/

#ifndef PANICBUSINESSACTION_H
#define PANICMAILBUSINESSACTION_H

#include "abstract_business_action.h"
#include "abstract_business_event.h"

#include <core/resource/resource_fwd.h>

class QnPanicBusinessAction: public QnAbstractBusinessAction
{
    typedef QnAbstractBusinessAction base_type;
public:
    explicit QnPanicBusinessAction(const QnBusinessParams &runtimeParams);

private:
};

typedef QSharedPointer<QnPanicBusinessAction> QnPanicBusinessActionPtr;

#endif  //PANICBUSINESSACTION_H
