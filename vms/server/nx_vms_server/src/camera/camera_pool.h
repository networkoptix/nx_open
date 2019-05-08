#pragma once

#include <memory>

#include <QtCore/QMap>

#include <nx/utils/singleton.h>
#include <nx/utils/thread/mutex.h>
#include <nx/vms/server/settings.h>

#include <core/resource/resource_fwd.h>


#include "camera_fwd.h"

class QnDataProviderFactory;

namespace nx { namespace vms::server { class Settings; } }
class QnResourcePool;

class VideoCameraLocker
{
public:
    VideoCameraLocker(QnVideoCameraPtr);
    virtual ~VideoCameraLocker();

    VideoCameraLocker(const VideoCameraLocker&) = delete;
    VideoCameraLocker& operator=(const VideoCameraLocker&) = delete;

private:
    QnVideoCameraPtr m_camera;
};

class QnVideoCameraPool: public  QObject
{
    Q_OBJECT
public:
    QnVideoCameraPool(
        const nx::vms::server::Settings& settings,
        QnDataProviderFactory* dataProviderFactory,
        QnResourcePool* resourcePool);
    virtual ~QnVideoCameraPool();

    void stop();

    /*!
        \return Object belongs to this pool
    */
    QnVideoCameraPtr getVideoCamera(const QnResourcePtr& res) const;
    QnVideoCameraPtr addVideoCamera(const QnResourcePtr& res);
    bool addVideoCamera(const QnResourcePtr& res, QnVideoCameraPtr camera);
    void removeVideoCamera(const QnResourcePtr& res);
    void updateActivity();

    std::unique_ptr<VideoCameraLocker> getVideoCameraLockerByResourceId(const QnUuid& id) const;

private:
    typedef QMap<QnResourcePtr, QnVideoCameraPtr> CameraMap;

    const nx::vms::server::Settings& m_settings;
    QnDataProviderFactory* m_dataProviderFactory = nullptr;
    QnResourcePool* m_resourcePool = nullptr;
    CameraMap m_cameras;
    mutable QnMutex m_mutex;
    bool m_isStopped = false;
};
