#include "events_serializer.h"

#include <QtCore/QDebug>
#include <QtCore/QElapsedTimer>

#include <nx/vms/event/action_parameters.h>
#include <nx/vms/event/event_parameters.h>

#include <utils/math/math.h>
#include <nx/fusion/model_functions.h>
#include <nx/utils/log/log.h>


inline int readInt(quint8* &curPtr)
{
    int val = *((int*)curPtr);
    curPtr += 4;
    return ntohl(val);
}

inline QnUuid readQnId(quint8* &curPtr)
{
    const QByteArray uuid = QByteArray::fromRawData((const char*)curPtr, 16);
    curPtr += 16;
    return QnUuid::fromRfc4122(uuid);
}

void QnEventSerializer::deserialize(nx::vms::event::ActionDataListPtr& eventsPtr, const QByteArray& data)
{
    QElapsedTimer t;
    t.restart();

    nx::vms::event::ActionDataList& events = *(eventsPtr.data());

    if (data.size() < 4)
        return;

    quint8* curPtr = (quint8*)data.data();
    const quint8* dataEnd = (quint8*) data.data() + data.size();
    int sz = readInt(curPtr);

    static const int kMaximumSize = 10000000;
    const bool validSize = qBetween(0, sz, kMaximumSize);

    NX_ASSERT(validSize, "Invalid binary data for events log. Ignoring");
    if (!validSize)
        return;

    events.resize(sz);
    for (int i = 0; i < sz; ++i)
    {
        nx::vms::event::ActionData& action = events[i];
        action.flags = readInt(curPtr);
        NX_EXPECT(curPtr <= dataEnd);
        action.actionType = (nx::vms::event::ActionType) readInt(curPtr);
        NX_EXPECT(curPtr <= dataEnd);
        action.businessRuleId = readQnId(curPtr);
        NX_EXPECT(curPtr <= dataEnd);
        action.aggregationCount = readInt(curPtr);
        NX_EXPECT(curPtr <= dataEnd);
        {
            int runTimeParamsLen = readInt(curPtr);
            bool validLength = curPtr <= dataEnd && runTimeParamsLen >= 0;
            NX_ASSERT(validLength);
            if (!validLength)
            {
                events.resize(i);
                return;
            }
            QByteArray ba = QByteArray::fromRawData((const char*)curPtr, runTimeParamsLen);
            action.eventParams = QnUbjson::deserialized<nx::vms::event::EventParameters>(ba);
            curPtr += runTimeParamsLen;
            NX_EXPECT(curPtr <= dataEnd);
        }

        {
            int actionParamsLen = readInt(curPtr);
            bool validLength = curPtr <= dataEnd && actionParamsLen >= 0;
            NX_ASSERT(validLength);
            if (!validLength)
            {
                events.resize(i);
                return;
            }
            QByteArray ba = QByteArray::fromRawData((const char*)curPtr, actionParamsLen);
            action.actionParams = QnUbjson::deserialized<nx::vms::event::ActionParameters>(ba);
            curPtr += actionParamsLen;
            NX_EXPECT(curPtr <= dataEnd);
        }

    }
    NX_LOG(lit("deserialize events log time= %1 msec").arg(t.elapsed()), cl_logDEBUG1);
}
