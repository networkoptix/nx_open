#ifndef __TIME_PERIOD_READER_H__
#define __TIME_PERIOD_READER_H__

#include <QObject>
#include <QRegion>
#include "api/VideoServerConnection.h"
#include "recording/time_period.h"
#include "core/resource/network_resource.h"

class QnTimePeriodReader: public QObject
{
    Q_OBJECT
public:
    QnTimePeriodReader(const QnVideoServerConnectionPtr &connection, QnNetworkResourcePtr resource, QObject *parent = NULL);
    int load(const QnTimePeriod &timePeriod, const QList<QRegion>& regions);

signals:
    void ready(const QnTimePeriodList &timePeriods, int handle);
    void failed(int status, int handle);

    void delayedReady(const QnTimePeriodList &timePeriods, int handle);
private slots:
    void at_replyReceived(int status, const QnTimePeriodList &timePeriods, int requstHandle);

private:
    int sendRequest(const QnTimePeriod& periodToLoad);

private:
    struct LoadingInfo 
    {
        LoadingInfo(const QnTimePeriod& _period, int _handle): period(_period), handle(_handle) {}
        QnTimePeriod period;
        int handle;
        QList<int> awaitingHandle;
    };

    static int m_fakeHandle;
    QMutex m_mutex;
    QnVideoServerConnectionPtr m_connection;
    QnNetworkResourcePtr m_resource;

    bool m_mergePeriods;
    QnTimePeriodList m_loadedPeriods;
    QList<LoadingInfo> m_loading;

    QnTimePeriodList m_loadedData;

    QList<QRegion> m_regions; // math only motion by specified region
};

typedef QSharedPointer<QnTimePeriodReader> QnTimePeriodUpdaterPtr;

#endif // __TIME_PERIOD_READER_H__
