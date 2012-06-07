#include "message.h"

QString QnMessage::objectNameLower() const
{
    if (objectName.isEmpty())
        return objectName;

    return objectName[0].toLower() + objectName.mid(1);
}

bool QnMessage::load(const QVariant& parsed)
{   
    dict = parsed.toMap();

    if (!dict.contains("type"))
        return false;

    eventType = dict["type"].toString();

    if (eventType != QN_MESSAGE_RES_CHANGE
        && eventType != QN_MESSAGE_RES_DELETE
        && eventType != QN_MESSAGE_RES_SETPARAM
        && eventType != QN_MESSAGE_RES_STATUS_CHANGE
        && eventType != QN_MESSAGE_RES_DISABLED_CHANGE
        && eventType != QN_MESSAGE_LICENSE_CHANGE
        && eventType != QN_MESSAGE_CAMERA_SERVER_ITEM
        && eventType != QN_MESSAGE_EMPTY
        && eventType != QN_MESSAGE_PING)
    {
        return false;
    }

    if (!dict.contains("seq_num"))
    {
        // version before 1.0.1
        // Do not use seqNumber tracking
        seqNumber = 0;
    } else {
        seqNumber = dict["seq_num"].toUInt();
    }

    if (eventType == QN_MESSAGE_LICENSE_CHANGE)
    {
        objectId = dict["id"].toString();
        return true;
    }

    if (eventType == QN_MESSAGE_CAMERA_SERVER_ITEM)
    {
        // will use dict
        // need to refactor a bit
        return true;
    }

    if (eventType == QN_MESSAGE_EMPTY || eventType == QN_MESSAGE_PING)
        return true;

    if (!dict.contains("resourceId"))
        return false;

    objectId = dict["resourceId"].toString();
    if (dict.contains("parentId"))
        parentId = dict["parentId"].toString();

    objectName = dict["objectName"].toString();

    if (dict.contains("resourceGuid"))
        resourceGuid = dict["resourceGuid"].toString();

    if (dict.contains("data"))
        data = dict["data"].toString();

    if (eventType == QN_MESSAGE_RES_SETPARAM)
    {
        if (!dict.contains("paramName") || !dict.contains("paramValue"))
            return false;

        paramName = dict["paramName"].toString();
        paramValue = dict["paramValue"].toString();

        return true;
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

