#pragma once

#include <memory>

#include <QtCore/QMap>

#include <nx/utils/singleton.h>
#include <nx/utils/thread/mutex.h>
#include <nx/vms/server/settings.h>

#include <nx/vms/server/resource/resource_fwd.h>
#include <core/resource/resource_fwd.h>

#include "camera_fwd.h"

class QnDataProviderFactory;

namespace nx { namespace vms::server { class Settings; } }
class QnResourcePool;

class VideoCameraLocker
{
public:
    VideoCameraLocker(nx::vms::server::VideoCameraPtr);
    virtual ~VideoCameraLocker();

    VideoCameraLocker(const VideoCameraLocker&) = delete;
    VideoCameraLocker& operator=(const VideoCameraLocker&) = delete;

private:
    nx::vms::server::VideoCameraPtr m_camera;
};

class QnVideoCameraPool: public QObject
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
    nx::vms::server::VideoCameraPtr getVideoCamera(const QnResourcePtr& res) const;
    nx::vms::server::VideoCameraPtr addVideoCamera(const nx::vms::server::resource::CameraPtr& res);
    bool addVideoCamera(const nx::vms::server::resource::CameraPtr& res, nx::vms::server::VideoCameraPtr camera);
    void removeVideoCamera(const QnResourcePtr& res);
    void updateActivity();

    std::unique_ptr<VideoCameraLocker> getVideoCameraLockerByResourceId(const QnUuid& id) const;

private:
    typedef QMap<QnResourcePtr, nx::vms::server::VideoCameraPtr> CameraMap;

    const nx::vms::server::Settings& m_settings;
    QnDataProviderFactory* m_dataProviderFactory = nullptr;
    QnResourcePool* m_resourcePool = nullptr;
    CameraMap m_cameras;
    mutable QnMutex m_mutex;
    bool m_isStopped = false;
};
