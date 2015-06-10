#pragma once

#include <QtCore/QObject>
#include <QtGui/QRegion>

#include <api/media_server_connection.h>

#include <core/resource/resource_fwd.h>
#include <core/resource/camera_bookmark_fwd.h>

#include <camera/data/camera_data_fwd.h>
#include <camera/loaders/abstract_camera_data_loader.h>

#include <recording/time_period.h>
#include <recording/time_period_list.h>

/**
 * Per-camera data loader that caches loaded data. 
 * Uses flat structure. Data is loaded with the most detailed level.
 * Source data period is solid, no spaces are allowed.
 */
class QnBookmarkCameraDataLoader: public QObject
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
    QnBookmarkCameraDataLoader(const QnVirtualCameraResourcePtr &camera, QObject *parent = NULL);
    
    virtual int load(const QString &filter, const qint64 resolutionMs);

    virtual void discardCachedData(const qint64 resolutionMs = 0);

signals:
    /**
     * This signal is emitted whenever motion periods were successfully loaded.
     *
     * \param data                      Full data loaded.
     * \param updatedPeriod             Source time period for the updated piece of data.
     * \param handle                    Request handle.
     */
    void ready(const QnAbstractCameraDataPtr &data, const QnTimePeriod &updatedPeriod, int handle);

    /**
     * This signal is emitted whenever the reader was unable to load motion periods.
     * 
     * \param status                    Error code.
     * \param handle                    Request handle.
     */
    void failed(int status, int handle);

private:
    int sendRequest(qint64 startTimeMs);
    void handleDataLoaded(int status, const QnAbstractCameraDataPtr &data, int requestHandle);
private:
    struct LoadingInfo 
    {
        /** Real loading handle, provided by the server connection object. */
        int handle;

        /** Starting time of the request. */
        qint64 startTimeMs;

        /** List of local (fake) handles for requests to this time period loader
         * that are waiting for the same time period to be loaded. */
        QList<int> waitingHandles;

        LoadingInfo();

        void clear();
    };

    const QnVirtualCameraResourcePtr m_camera;

    QString m_filter;
    
    LoadingInfo m_loading;

    /** Loaded data. */
    QnAbstractCameraDataPtr m_loadedData;

};
