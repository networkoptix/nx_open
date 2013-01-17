#ifndef QN_MESSAGE_H
#define QN_MESSAGE_H

#include <QtCore/QMetaType>

#include <utils/common/qnid.h>

#include <core/resource/resource.h>
#include <core/resource/camera_history.h>

#include <events/business_event_rule.h>
#include <events/abstract_business_action.h>

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
        Message_Type_BroadcastBusinessAction = 10
    };

    QString toString( Message_Type val );
}

struct QnMessage
{
    Qn::Message_Type eventType;
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
    QnAbstractBusinessActionPtr businessAction;

    bool load(const pb::Message& message);

    static quint32 nextSeqNumber(quint32 seqNumber);
};

Q_DECLARE_METATYPE(QnMessage);

#endif // QN_MESSAGE_H
