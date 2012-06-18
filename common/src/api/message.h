#ifndef QN_MESSAGE_H
#define QN_MESSAGE_H

#include "utils/common/qnid.h"

static const char* QN_MESSAGE_EMPTY                 = "EE";
static const char* QN_MESSAGE_PING                  = "PE";
static const char* QN_MESSAGE_RES_CHANGE            = "RC";
static const char* QN_MESSAGE_RES_STATUS_CHANGE     = "RSC";
static const char* QN_MESSAGE_RES_DISABLED_CHANGE   = "RDC";
static const char* QN_MESSAGE_RES_DELETE            = "RD";
static const char* QN_MESSAGE_LICENSE_CHANGE        = "LC";
static const char* QN_MESSAGE_CAMERA_SERVER_ITEM    = "CSI";

struct QnMessage
{
    QString eventType;
    quint32 seqNumber;

    QString objectName;
    QnId objectId;
    QnId parentId;
    QString resourceGuid;

    QMap<QString, QVariant> dict;

    bool load(const QVariant& parsed);
    QString objectNameLower() const;

    static quint32 nextSeqNumber(quint32 seqNumber);
};
Q_DECLARE_METATYPE(QnMessage);

#endif // QN_MESSAGE_H
