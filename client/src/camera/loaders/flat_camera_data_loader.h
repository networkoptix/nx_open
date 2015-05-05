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
class QnFlatCameraDataLoader: public QnAbstractCameraDataLoader
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
    QnFlatCameraDataLoader(const QnMediaServerConnectionPtr &connection, const QnNetworkResourcePtr &resource, Qn::CameraDataType dataType, QObject *parent = NULL);

    /**
     * Creates a new time period loader for the given camera resource. Returns NULL
     * pointer in case loader cannot be created.
     * 
     * \param camera                    Camera resource to create time period loader for.
     * \param parent                    Parent object for the loader to create.
     * \returns                         Newly created time period loader.
     */
    static QnFlatCameraDataLoader *newInstance(const QnMediaServerResourcePtr &server, const QnNetworkResourcePtr &camera, Qn::CameraDataType dataType, QObject *parent = NULL);
    
    virtual int load(const QString &filter, const qint64 resolutionMs) override;

    virtual void discardCachedData(const qint64 resolutionMs = 0) override;

private slots:
    void at_timePeriodsReceived(int status, const QnTimePeriodList &timePeriods, int requestHandle);

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

    /** Video server connection that this loader uses. */
    QnMediaServerConnectionPtr m_connection;

    QString m_filter;
    
    LoadingInfo m_loading;

    /** Loaded data. */
    QnAbstractCameraDataPtr m_loadedData;

};
