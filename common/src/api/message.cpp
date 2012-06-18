#include "message.h"

#include "message.pb.h"

bool QnMessage::load(const pb::Message &message)
{
    eventType = (Message_Type)message.type();
    seqNumber = message.seqnumber();

    switch (eventType)
    {
        case pb::Message_Type_ResourceChange:
        case pb::Message_Type_ResourceDisabledChange:
        case pb::Message_Type_ResourceStatusChange:
        {
            // TODO:
            //resource =;
            break;
        }
        case pb::Message_Type_License:
        {
            //license =;
            break;
        }
        case pb::Message_Type_CameraServerItem:
        {
            break;
        }
        case pb::Message_Type_ResourceDelete:
        {
            break;
        }
    }

    return true;
}

quint32 QnMessage::nextSeqNumber(quint32 seqNumber)
{
    seqNumber++;

    if (seqNumber == 0)
        seqNumber++;

    return seqNumber;
}

