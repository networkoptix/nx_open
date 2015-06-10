#pragma once

#include <QtCore/QObject>
#include <QtGui/QRegion>

#include <api/media_server_connection.h>

#include <core/resource/resource_fwd.h>
#include <core/resource/camera_bookmark.h>

#include <camera/data/camera_data_fwd.h>
#include <camera/loaders/abstract_camera_data_loader.h>

#include <recording/time_period.h>
#include <recording/time_period_list.h>

/**
 * Per-camera data loader that caches loaded data. 
 */
class QnBookmarksLoader: public QObject
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
    QnBookmarksLoader(const QnVirtualCameraResourcePtr &camera, QObject *parent = NULL);
    
    void load(const QnTimePeriod &period);

    QnCameraBookmarkList bookmarks() const;

    void discardCachedData();

signals:
    /**
     * This signal is emitted whenever bookmarks were successfully loaded.
     *
     * \param data                      Full data loaded.
     * \param updatedPeriod             Source time period for the updated piece of data.
     * \param handle                    Request handle.
     */
    void bookmarksChanged(const QnCameraBookmarkList &bookmarks);

private:
    void sendRequest(const QnTimePeriod &period);
    void handleDataLoaded(int status, const QnCameraBookmarkList &data, int requestHandle);
private:

    const QnVirtualCameraResourcePtr m_camera;
    
    /** Loaded data. */
    //TODO: #GDM #Bookmarks replace structure with indexed tree
    QnCameraBookmarkList m_loadedData;

};
