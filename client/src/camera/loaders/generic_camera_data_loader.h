#ifndef QN_GENERIC_CAMERA_DATA_LOADER_H
#define QN_GENERIC_CAMERA_DATA_LOADER_H

#include <QtCore/QObject>
#include <QtGui/QRegion>

#include <api/media_server_connection.h>

#include <core/resource/resource_fwd.h>
#include <core/resource/camera_bookmark_fwd.h>

#include <camera/abstract_camera_data.h>
#include <camera/loaders/abstract_camera_data_loader.h>

#include <recording/time_period_list.h>

/**
 * Per-camera data loader that caches loaded data. 
 */
class QnGenericCameraDataLoader: public QnAbstractCameraDataLoader
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
    QnGenericCameraDataLoader(const QnMediaServerConnectionPtr &connection, const QnNetworkResourcePtr &resource, Qn::CameraDataType dataType, QObject *parent = NULL);

    /**
     * Creates a new time period loader for the given camera resource. Returns NULL
     * pointer in case loader cannot be created.
     * 
     * \param resource                  Camera resource to create time period loader for.
     * \param parent                    Parent object for the loader to create.
     * \returns                         Newly created time period loader.
     */
    static QnGenericCameraDataLoader *newInstance(const QnMediaServerResourcePtr &serverResource, const QnResourcePtr &resource, Qn::CameraDataType dataType, QObject *parent = NULL);
    
    virtual int load(const QnTimePeriod &timePeriod, const QString &filter) override;

    virtual void discardCachedData() override;

private slots:
    void at_timePeriodsReceived(int status, const QnTimePeriodList &timePeriods, int requestHandle);
    void at_bookmarksReceived(int status, const QnCameraBookmarkList &bookmarks, int requestHandle);

private:
    int sendRequest(const QnTimePeriod &periodToLoad);
    void handleDataLoaded(int status, const QnAbstractCameraDataPtr &data, int requstHandle);
    void updateLoadedPeriods(QnTimePeriod loadedPeriod);
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

    /** Video server connection that this loader uses. */
    QnMediaServerConnectionPtr m_connection;

    QString m_filter;
    
    QList<LoadingInfo> m_loading;

    QnAbstractCameraDataPtr m_loadedData;
    QnTimePeriodList m_loadedPeriods;
};

#endif // QN_GENERIC_CAMERA_DATA_LOADER_H
