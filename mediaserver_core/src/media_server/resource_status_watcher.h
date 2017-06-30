#ifndef __RESOURCE_STATUS_WATCHER_H__
#define __RESOURCE_STATUS_WATCHER_H__

#include <QObject>
#include <QSet>

#include <core/resource/resource_fwd.h>
#include <nx_ec/ec_api.h>
#include <nx/utils/singleton.h>

class QnResourceStatusWatcher:
    public QObject,
    public QnCommonModuleAware,
    public Singleton<QnResourceStatusWatcher>
{
    Q_OBJECT
public:
    QnResourceStatusWatcher(QnCommonModule* commonModule);
    void updateResourceStatus(const QnResourcePtr& resource);
signals:
    void statusChanged(const QnResourcePtr& resource);
private slots:
    void at_resource_statusChanged(const QnResourcePtr& resource, Qn::StatusChangeReason reason);
    void updateResourceStatusInternal(const QnResourcePtr& resource);
private:
    bool isSetStatusInProgress(const QnResourcePtr &resource);
    void updateResourceStatusAsync(const QnResourcePtr &resource);
    void requestFinished2( int reqID, ec2::ErrorCode errCode, const QnUuid& id );
private:
    QSet<QnUuid> m_awaitingSetStatus;
    QSet<QnUuid> m_setStatusInProgress;
};

#endif // __RESOURCE_STATUS_WATCHER_H__
