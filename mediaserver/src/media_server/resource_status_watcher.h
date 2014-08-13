#ifndef __RESOURCE_STATUS_WATCHER_H__
#define __RESOURCE_STATUS_WATCHER_H__

#include <QObject>
#include <QSet>
#include <core/resource/resource.h>
#include <nx_ec/ec_api.h>

class QnResourceStatusWatcher : public QObject
{
    Q_OBJECT
public:
    QnResourceStatusWatcher();
    private slots:
        void at_resource_statusChanged(const QnResourcePtr& resource);
private:
    bool isSetStatusInProgress(const QnResourcePtr &resource);
    void updateResourceStatusAsync(const QnResourcePtr &resource);
    void requestFinished2( int reqID, ec2::ErrorCode errCode, const QUuid& id );
private:
    QSet<QUuid> m_awaitingSetStatus;
    QSet<QUuid> m_setStatusInProgress;
};

#endif // __RESOURCE_STATUS_WATCHER_H__
