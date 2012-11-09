#ifndef QN_WORKBENCH_SERVER_TIME_WATCHER_H
#define QN_WORKBENCH_SERVER_TIME_WATCHER_H

#include <QtCore/QObject>
#include <QtCore/QHash>
#include <QtCore/QDateTime>
#include <QtCore/QBasicTimer>

#include <utils/common/math.h>
#include <core/resource/resource_fwd.h>

#include <ui/workbench/workbench_context_aware.h>

class QnWorkbenchServerTimeWatcher: public QObject, public QnWorkbenchContextAware {
    Q_OBJECT;

    typedef QObject base_type;

public:
    QnWorkbenchServerTimeWatcher(QObject *parent);
    virtual ~QnWorkbenchServerTimeWatcher();

    qint64 utcOffset(const QnMediaServerResourcePtr &server, qint64 defaultValue = Qn::InvalidUtcOffset) const;

    qint64 localOffset(const QnMediaServerResourcePtr &server, qint64 defaultValue = Qn::InvalidUtcOffset) const;

signals:
    void offsetsChanged();

protected:
    virtual void timerEvent(QTimerEvent *event) override;

private:
    void updateServerTime(const QnMediaServerResourcePtr &server);

private slots:
    void at_server_serverIFFound();

    void at_resourcePool_resourceAdded(const QnResourcePtr &resource);
    void at_resourcePool_resourceRemoved(const QnResourcePtr &resource);

    void at_replyReceived(int status, const QDateTime &dateTime, int utcOffset, int handle);

private:
    QBasicTimer m_timer;
    QHash<int, QnMediaServerResourcePtr> m_resourceByHandle;
    QHash<QnMediaServerResourcePtr, qint64> m_utcOffsetByResource;
};

#endif // QN_WORKBENCH_SERVER_TIME_WATCHER_H
