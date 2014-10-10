#include "events_serializer.h"

#include <QtCore/QDebug>
#include <QtCore/QElapsedTimer>


inline int readInt(quint8* &curPtr)
{
    int val = *((int*)curPtr);
    curPtr += 4;
    return ntohl(val);
}

inline QnUuid readQnId(quint8* &curPtr)
{
    const QByteArray uuid = QByteArray::fromRawData((const char*) curPtr, 16);
    curPtr += 16;
    return QnUuid::fromRfc4122(uuid);
}

void QnEventSerializer::deserialize(QnBusinessActionDataListPtr& eventsPtr, const QByteArray& data)
{
    QElapsedTimer t;
    t.restart();

    QnBusinessActionDataList& events = *(eventsPtr.data());


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
        QnBusinessActionData& action = events[i];
        action.setFlags(readInt(curPtr));
        action.setActionType((QnBusiness::ActionType) readInt(curPtr));
        action.setBusinessRuleId(readQnId(curPtr));
        action.setAggregationCount(readInt(curPtr));
        int runTimeParamsLen = readInt(curPtr);
        QByteArray ba = QByteArray::fromRawData((const char*)curPtr, runTimeParamsLen);
        action.setRuntimeParams(QnBusinessEventParameters::unpack(ba));
        curPtr += runTimeParamsLen;

        int actionParamsLen = readInt(curPtr);
        ba = QByteArray::fromRawData((const char*)curPtr, actionParamsLen);
        action.setParams(QnBusinessActionParameters::unpack(ba));
        curPtr += actionParamsLen;

    }

    qDebug() << "deserialize events log time=" << t.elapsed() << "msec";
}
