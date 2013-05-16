#include "events_serializer.h"

#include <QDebug>


inline int readInt(quint8* &curPtr)
{
    int val = *((int*)curPtr);
    curPtr += 4;
    return val;
}

void QnEventSerializer::deserialize(QnLightBusinessActionVectorPtr& eventsPtr, const QByteArray& data)
{
    QTime t;
    t.restart();

    QnLightBusinessActionVector& events = *(eventsPtr.data());

    if (data.size() < 4)
        return;

    quint8* curPtr = (quint8*) data.data();
    int sz = readInt(curPtr);
    if (sz < 0 || sz > 10000000) {
        qWarning() << "Invalid binary data for events log. Ignoring";
        return;
    }

    events.resize(sz);
    for (int i = 0; i < sz; ++i) {
        QnLightBusinessAction& action = events[i];
        action.setActionType((BusinessActionType::Value) readInt(curPtr));
        action.setBusinessRuleId(readInt(curPtr));
        action.setAggregationCount(readInt(curPtr));
        int runTimeParamsLen = readInt(curPtr);
        QByteArray ba = QByteArray::fromRawData((const char*)curPtr, runTimeParamsLen);
        action.setRuntimeParams(QnBusinessEventParameters::deserialize(ba));
        curPtr += runTimeParamsLen;
    }

    qWarning() << "deserialize time=" << t.elapsed() << "msec";
}
