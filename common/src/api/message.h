#ifndef QN_MESSAGE_H
#define QN_MESSAGE_H

#include <QtCore/QMetaType>

#include <utils/common/id.h>

#include <api/model/kvpair.h>

#include <core/resource/resource_type.h>
#include <core/resource/resource_fwd.h>
#include <core/resource/camera_history.h>
#include <core/resource/videowall_control_message.h>

#include <business/business_event_rule.h>
#include <business/actions/abstract_business_action.h>

#include <licensing/license.h>

namespace pb {
    class Message;
}

namespace Qn {
    enum Message_Type {
        Message_Type_Initial = 0,
        Message_Type_Ping = 1,
        Message_Type_ResourceChange = 2,
        Message_Type_ResourceDelete = 3,
        Message_Type_ResourceStatusChange = 4,
        Message_Type_ResourceDisabledChange = 5,
        Message_Type_License = 6,
        Message_Type_CameraServerItem = 7,
        Message_Type_BusinessRuleInsertOrUpdate = 8,
        Message_Type_BusinessRuleDelete = 9,
        Message_Type_BroadcastBusinessAction = 10,
        Message_Type_FileAdd = 11,
        Message_Type_FileRemove = 12,
        Message_Type_FileUpdate = 13,
        Message_Type_RuntimeInfoChange = 14,
        Message_Type_BusinessRuleReset = 15,
        Message_Type_KvPairChange = 16,
        Message_Type_KvPairDelete = 17,
        Message_Type_Command = 18,
        Message_Type_VideoWallControl = 19,

        Message_Type_Count
    };

    QString toString( Message_Type val );
}

class QnMessage
{
public:
    QnMessage(): messageType(Qn::Message_Type_Initial), seqNumber(0), resourceDisabled(false), resourceStatus(QnResource::Online) {}

    enum Command {
        Reboot = 0
    };

    Qn::Message_Type messageType;
    quint32 seqNumber;

    QnResourcePtr resource;

    // These fields are temporary and caused by
    // heavy-weightness of QnResource
    QnId resourceId;
    QString resourceGuid;
    bool resourceDisabled;
    QnResource::Status resourceStatus;

    QnLicensePtr license;
    QnCameraHistoryItemPtr cameraServerItem;
    QnBusinessEventRulePtr businessRule;
    QnBusinessEventRuleList businessRules;
    QnAbstractBusinessActionPtr businessAction;

    QnResourceTypeList resourceTypes;
    QnResourceList resources;
    QnLicenseList licenses;
    QnCameraHistoryList cameraServerItems;

    QString systemName;
    QByteArray sessionKey;
    QList<QByteArray> mainHardwareIds;
    QList<QByteArray> compatibleHardwareIds;


    QString filename;
    QString publicIp;

    QnKvPairListsById kvPairs;

    Command command;

    QnVideoWallControlMessage videoWallControlMessage;

    bool load(const pb::Message& message);

    static quint32 nextSeqNumber(quint32 seqNumber);
};

Q_DECLARE_METATYPE(QnMessage)

#endif // QN_MESSAGE_H
