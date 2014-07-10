#ifndef QN_SYNC_TIME_H
#define QN_SYNC_TIME_H

#include <QtCore/QObject>
#include <QtCore/QDateTime>
#include <QtCore/QMutex>

class QnSyncTimeTask;

/** 
 * Time provider that is synchronized with Enterprise Controller.
 */
class QnSyncTime: public QObject {
    Q_OBJECT;

public:
    QnSyncTime();
    virtual ~QnSyncTime();

    static QnSyncTime* instance();

    qint64 currentMSecsSinceEpoch();
    qint64 currentUSecsSinceEpoch();
    QDateTime currentDateTime();

    void reset();

public slots:
    //!Sets new synchronized time to \a newTime
    void updateTime( qint64 newTime );

signals:
    /**
     * This signal is emitted whenever time on EC changes. 
     */
    void timeChanged();

private:
    QTime m_timer;
    qint64 m_lastReceivedTime;
    bool m_requestSended;
    QnSyncTimeTask* m_gotTimeTask;
    QMutex m_mutex;
    qint64 m_lastWarnTime;
    qint64 m_lastLocalTime;

    friend class QnSyncTimeTask;
};

#define qnSyncTime (QnSyncTime::instance())

#endif // QN_SYNC_TIME_H
