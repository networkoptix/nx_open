#ifndef VIDEOWALL_INSTANCE_STATUS_H
#define VIDEOWALL_INSTANCE_STATUS_H

#include <QtCore/QMetaType>
#include <QtCore/QUuid>

/**
 * This class contains the status of the running videowall instance.
 */
struct QnVideowallInstanceStatus {
public:
    QnVideowallInstanceStatus():
        online(false)
    {}

    QnVideowallInstanceStatus(const QUuid &videowallGuid, const QUuid &instanceGuid, bool online):
        videowallGuid(videowallGuid), instanceGuid(instanceGuid), online(online)
    {}

    QUuid videowallGuid;
    QUuid instanceGuid;
    bool online;
};

Q_DECLARE_METATYPE(QnVideowallInstanceStatus)


#endif // VIDEOWALL_INSTANCE_STATUS_H
