/**********************************************************
* 29 nov 2012
* a.kolesnikov
***********************************************************/
#ifndef QN_PANIC_BUSINESS_ACTION_H
#define QN_PANIC_BUSINESS_ACTION_H

#include <business/actions/abstract_business_action.h>
#include <business/events/abstract_business_event.h>

#include <core/resource/resource_fwd.h>

class QnPanicBusinessAction: public QnAbstractBusinessAction
{
    typedef QnAbstractBusinessAction base_type;
public:
    explicit QnPanicBusinessAction(const QnBusinessEventParameters &runtimeParams);
};

#endif  //QN_PANIC_BUSINESS_ACTION_H
