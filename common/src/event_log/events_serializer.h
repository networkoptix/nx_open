#ifndef __EVENTS_SERIALIZER_H__
#define __EVENTS_SERIALIZER_H__

#include "business/actions/abstract_business_action.h"

class QnEventSerializer
{
public:
    static void deserialize(QnBusinessActionDataListPtr& events, const QByteArray& data);
};

#endif // __EVENTS_SERIALIZER_H__