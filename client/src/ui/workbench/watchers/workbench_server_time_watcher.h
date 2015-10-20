#pragma once

#include <QtCore/QObject>
#include <QtCore/QHash>

#include <core/resource/resource_fwd.h>

#include <utils/common/connective.h>

struct QnTimeReply;

class QnWorkbenchServerTimeWatcher: public Connective<QObject> {
    Q_OBJECT;

    typedef Connective<QObject> base_type;

public:
    QnWorkbenchServerTimeWatcher(QObject *parent);
    virtual ~QnWorkbenchServerTimeWatcher();

    qint64 utcOffset(const QnMediaServerResourcePtr &server, qint64 defaultValue = Qn::InvalidUtcOffset) const;
    qint64 utcOffset(const QnMediaResourcePtr &resource, qint64 defaultValue = Qn::InvalidUtcOffset) const;
   
    qint64 displayOffset(const QnMediaResourcePtr &resource) const;

    QDateTime serverTime(const QnMediaServerResourcePtr &server, qint64 msecsSinceEpoch) const;
    QDateTime displayTime(qint64 msecsSinceEpoch) const;
signals:
    void displayOffsetsChanged();

private:
    void sendRequest(const QnMediaServerResourcePtr &server);
   
    qint64 localOffset(const QnMediaResourcePtr &resource, qint64 defaultValue = Qn::InvalidUtcOffset) const;
private slots:
    void at_resourcePool_resourceAdded(const QnResourcePtr &resource);
    void at_resourcePool_resourceRemoved(const QnResourcePtr &resource);

    void at_replyReceived(int status, const QnTimeReply &reply, int handle);

private:
    QTimer* m_timer;
    QHash<int, QnMediaServerResourcePtr> m_resourceByHandle;
    QHash<QnMediaServerResourcePtr, qint64> m_utcOffsetByResource;
};
