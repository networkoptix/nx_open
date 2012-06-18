#ifndef QN_MESSAGE_H
#define QN_MESSAGE_H

#include "utils/common/qnid.h"
#include "core/resource/resource.h"
#include "licensing/license.h"
#include "core/resource/camera_history.h"

static const char* QN_MESSAGE_RES_CHANGE            = "RC";
static const char* QN_MESSAGE_RES_STATUS_CHANGE     = "RSC";
static const char* QN_MESSAGE_RES_DISABLED_CHANGE   = "RDC";
static const char* QN_MESSAGE_RES_DELETE            = "RD";
static const char* QN_MESSAGE_LICENSE_CHANGE        = "LC";
static const char* QN_MESSAGE_CAMERA_SERVER_ITEM    = "CSI";

namespace pb {
    class Message;
}

// Copied from message.pb.h
// TODO: Somehow avoid duplicaton of this enum
enum Message_Type {
  Message_Type_Initial = 0,
  Message_Type_Ping = 1,
  Message_Type_ResourceChange = 2,
  Message_Type_ResourceDelete = 3,
  Message_Type_ResourceStatusChange = 4,
  Message_Type_ResourceDisabledChange = 5,
  Message_Type_License = 6,
  Message_Type_CameraServerItem = 7
};

struct QnMessage
{
    Message_Type eventType;
    quint32 seqNumber;

    QnResourcePtr resource;
    QnLicensePtr license;
    QnCameraHistoryItemPtr cameraServerItem;

    bool load(const pb::Message& message);

    static quint32 nextSeqNumber(quint32 seqNumber);
};
Q_DECLARE_METATYPE(QnMessage);

#endif // QN_MESSAGE_H
