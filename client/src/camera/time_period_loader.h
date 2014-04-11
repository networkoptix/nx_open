#ifndef QN_TIME_PERIOD_LOADER_H
#define QN_TIME_PERIOD_LOADER_H

#include <QtCore/QObject>
#include <QtGui/QRegion>

#include <api/media_server_connection.h>
#include <recording/time_period_list.h>
#include <core/resource/network_resource.h>
#include "abstract_time_period_loader.h"

/**
 * Per-camera time period loader that caches loaded time periods. 
 */
class QnTimePeriodLoader: public QnAbstractTimePeriodLoader
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
    QnTimePeriodLoader(const QnMediaServerConnectionPtr &connection, QnNetworkResourcePtr resource, QObject *parent = NULL);

    /**
     * Creates a new time period loader for the given camera resource. Returns NULL
     * pointer in case loader cannot be created.
     * 
     * \param resource                  Camera resource to create time period loader for.
     * \param parent                    Parent object for the loader to create.
     * \returns                         Newly created time period loader.
     */
    static QnTimePeriodLoader *newInstance(QnMediaServerResourcePtr serverResource, QnResourcePtr resource, QObject *parent = NULL);
    
    virtual int load(const QnTimePeriod &timePeriod, const QList<QRegion> &motionRegions = QList<QRegion>()) override;

    virtual void discardCachedData() override;

private slots:
    void at_replyReceived(int status, const QnTimePeriodList &timePeriods, int requstHandle);

private:
    int sendRequest(const QnTimePeriod &periodToLoad);

private:
    struct LoadingInfo 
    {
        LoadingInfo(const QnTimePeriod &period, int handle): period(period), handle(handle) {}

        /** Period for which chunks are being loaded. */
        QnTimePeriod period;

        /** Real loading handle, provided by the server connection object. */
        int handle;

        /** List of local (fake) handles for requests to this time period loader
         * that are waiting for the same time period to be loaded. */
        QList<int> waitingHandles;
    };

    /** Thread safety. */
    mutable QMutex m_mutex;

    /** Video server connection that this loader uses. */
    QnMediaServerConnectionPtr m_connection;

    QnTimePeriodList m_loadedPeriods;
    QList<LoadingInfo> m_loading;

    QnTimePeriodList m_loadedData;

    QList<QRegion> m_motionRegions;
};

#endif // QN_TIME_PERIOD_LOADER_H
