#pragma once

#include <array>

#include <QtCore/QObject>
#include <QtCore/QElapsedTimer>

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
    void queueToLoad(const QnTimePeriod &period);
    void processLoadQueue();

    void sendRequest(const QnTimePeriod &period);
    Q_SLOT void handleDataLoaded(int status, const QnCameraBookmarkList &data, int requestHandle);
private:

    const QnVirtualCameraResourcePtr m_camera;
    
    /** Loaded data. */
    //TODO: #GDM #Bookmarks replace structure with indexed tree
    QnCameraBookmarkList m_loadedData;

    QElapsedTimer m_timer;

    QnTimePeriod m_queuedPeriod;
};
