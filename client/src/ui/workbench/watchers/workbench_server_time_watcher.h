#ifndef QN_WORKBENCH_SERVER_TIME_WATCHER_H
#define QN_WORKBENCH_SERVER_TIME_WATCHER_H

#include <QtCore/QObject>
#include <QtCore/QHash>
#include <QtCore/QDateTime>

#include <utils/common/math.h>
#include <core/resource/resource_fwd.h>

#include <ui/workbench/workbench_context_aware.h>

#define InvalidUtcOffset _id(INT64_MAX)

class QnWorkbenchServerTimeWatcher: public QObject, public QnWorkbenchContextAware {
    Q_OBJECT;
public:
    QnWorkbenchServerTimeWatcher(QObject *parent);
    virtual ~QnWorkbenchServerTimeWatcher();

    qint64 utcOffset(const QnMediaServerResourcePtr &server, qint64 defaultValue = Qn::InvalidUtcOffset) const;

    qint64 localOffset(const QnMediaServerResourcePtr &server, qint64 defaultValue = Qn::InvalidUtcOffset) const;

signals:
    void offsetsChanged();

private slots:
    void at_server_serverIFFound(const QnMediaServerResourcePtr &server);
    void at_server_serverIFFound();

    void at_resourcePool_resourceAdded(const QnResourcePtr &resource);
    void at_resourcePool_resourceRemoved(const QnResourcePtr &resource);

    void at_replyReceived(int status, const QDateTime &dateTime, int utcOffset, int handle);

private:
    QHash<int, QnMediaServerResourcePtr> m_resourceByHandle;
    QHash<QnMediaServerResourcePtr, qint64> m_utcOffsetByResource;
};

#endif // QN_WORKBENCH_SERVER_TIME_WATCHER_H
