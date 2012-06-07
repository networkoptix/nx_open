#ifndef QN_TIME_PERIOD_LOADER_H
#define QN_TIME_PERIOD_LOADER_H

#include <QtCore/QObject>
#include <QtGui/QRegion>

#include "api/video_server_connection.h"
#include "recording/time_period.h"
#include "core/resource/network_resource.h"

/**
 * Per-camera time period loader that caches loaded time periods. 
 */
class QnTimePeriodLoader: public QObject
{
    Q_OBJECT
public:
    /**
     * Constructor.
     * 
     * \param connection                Video server connection to use.
     * \param resource                  Network resource representing the camera to work with.
     * \param parent                    Parent object.
     */
    QnTimePeriodLoader(const QnVideoServerConnectionPtr &connection, QnNetworkResourcePtr resource, QObject *parent = NULL);

    /**
     * Creates a new time period loader for the given camera resource. Returns NULL
     * pointer in case loader cannot be created.
     * 
     * \param resource                  Camera resource to create time period loader for.
     * \param parent                    Parent object for the loader to create.
     * \returns                         Newly created time period loader.
     */
    static QnTimePeriodLoader *newInstance(QnResourcePtr resource, QObject *parent = NULL);
    
    /**
     * \param timePeriod                Time period to get motion periods for.
     * \param motionRegions             List of regions to look for motion, one region per video channel.
     *                                  If empty list is provided, then recorded time periods will be returned.
     * \returns                         Request handle.
     * 
     * \note                            This function is thread-safe.
     */
    int load(const QnTimePeriod &timePeriod, const QList<QRegion> &motionRegions = QList<QRegion>());

    /**
     * \returns                         Network resource representing the camera that this
     *                                  loader works with.
     * 
     * \note                            This function is thread-safe.
     */
    QnNetworkResourcePtr resource() const;

signals:
    /**
     * This signal is emitted whenever motion periods were successfully loaded from server.
     *
     * \param timePeriods               Loaded motion periods.
     * \param handle                    Request handle.
     */
    void ready(const QnTimePeriodList &timePeriods, int handle);

    /**
     * This signal is emitted whenever the reader was unable to load motion periods from server.
     * 
     * \param status                    Error code.
     * \param handle                    Request handle.
     */
    void failed(int status, int handle);

private slots:
    void at_replyReceived(int status, const QnTimePeriodList &timePeriods, int requstHandle);

signals:
    void delayedReady(const QnTimePeriodList &timePeriods, int handle);

private:
    int sendRequest(const QnTimePeriod &periodToLoad);

private:
    struct LoadingInfo 
    {
        LoadingInfo(const QnTimePeriod &period, int handle): period(period), handle(handle) {}
        QnTimePeriod period;
        int handle;
        QList<int> waitingHandles;
    };

    mutable QMutex m_mutex;
    QnVideoServerConnectionPtr m_connection;
    QnNetworkResourcePtr m_resource;

    bool m_mergePeriods;
    QnTimePeriodList m_loadedPeriods;
    QList<LoadingInfo> m_loading;

    QnTimePeriodList m_loadedData;

    QList<QRegion> m_motionRegions;
};

#endif // QN_TIME_PERIOD_LOADER_H
