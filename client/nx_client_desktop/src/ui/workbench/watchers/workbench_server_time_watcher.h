#pragma once

#include <QtCore/QObject>
#include <QtCore/QHash>

#include <client_core/connection_context_aware.h>

#include <common/common_globals.h>

#include <core/resource/resource_fwd.h>

#include <utils/common/connective.h>

struct QnTimeReply;

class QnWorkbenchServerTimeWatcher: public Connective<QObject>, public QnConnectionContextAware
{
    Q_OBJECT;

    typedef Connective<QObject> base_type;

public:
    QnWorkbenchServerTimeWatcher(QObject *parent);
    virtual ~QnWorkbenchServerTimeWatcher();

    /** Current utc offset on the given server. Depends on server local timezone. */
    qint64 utcOffset(const QnMediaServerResourcePtr &server, qint64 defaultValue = Qn::InvalidUtcOffset) const;

    /** Utc offset for the given resource. Supports cameras (current parent server offset is used)
     *  and exported files (offset is stored inside).
     */
    qint64 utcOffset(const QnMediaResourcePtr &resource, qint64 defaultValue = Qn::InvalidUtcOffset) const;

    /** Offset value, used in gui elements. Depends on user time mode settings. */
    qint64 displayOffset(const QnMediaResourcePtr &resource) const;

    /** Current time on the given server. Really, sync time is used, but with correct utc offset. */
    QDateTime serverTime(const QnMediaServerResourcePtr &server, qint64 msecsSinceEpoch) const;

    /** Time value, than takes into account user time mode settings. */
    QDateTime displayTime(qint64 msecsSinceEpoch) const;
signals:
    void displayOffsetsChanged();

private:
    void sendRequest(const QnMediaServerResourcePtr &server);

    qint64 localOffset(const QnMediaResourcePtr &resource, qint64 defaultValue = Qn::InvalidUtcOffset) const;
private slots:
    void at_resourcePool_resourceAdded(const QnResourcePtr &resource);
    void at_resourcePool_resourceRemoved(const QnResourcePtr &resource);

private:
    QTimer* m_timer;
    QHash<QnMediaServerResourcePtr, qint64> m_utcOffsetByResource;
};
