// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "virtual_camera_preparer.h"

#include <QtCore/QFile>
#include <QtCore/QFileInfo>

#include <api/server_rest_connection.h>
#include <core/resource/avi/avi_archive_delegate.h>
#include <core/resource/avi/avi_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/security_cam_resource.h>
#include <nx/utils/guarded_callback.h>
#include <nx/vms/client/desktop/utils/server_request_storage.h>
#include <recording/time_period_list.h>
#include <utils/common/synctime.h>

#include "virtual_camera_payload.h"

namespace nx::vms::client::desktop {

namespace {

bool canUpload(const QnTimePeriod& local, const QnTimePeriod& remote)
{
    const qint64 kMaxIgnoredOverlapMs = 200;

    if (local == remote)
        return true;

    QnTimePeriod remoteExtended = remote;
    remoteExtended.startTimeMs -= kMaxIgnoredOverlapMs;
    remoteExtended.durationMs += 2 * kMaxIgnoredOverlapMs;

    return remoteExtended.contains(local);
}

QnTimePeriod shrinkPeriod(const QnTimePeriod& local, const QnTimePeriodList& remote)
{
    QnTimePeriodList locals{local};
    locals.excludeTimePeriods(remote);

    if (!locals.empty())
        return locals[0];
    return QnTimePeriod();
}

} // namespace


struct VirtualCameraPreparer::Private
{
    Private(const QnSecurityCamResourcePtr& camera):
        camera(camera)
    {
    }

    QnSecurityCamResourcePtr camera;
    QnTimePeriodList queuePeriods;
    VirtualCameraUpload upload;
    QVector<int> localByRemoteIndex;

    ServerRequestStorage requests;
};

VirtualCameraPreparer::VirtualCameraPreparer(const QnSecurityCamResourcePtr& camera, QObject* parent):
    base_type(parent),
    d(new Private(camera))
{
    connect(this, &VirtualCameraPreparer::finishedLater, this,
        &VirtualCameraPreparer::finished, Qt::QueuedConnection);
}

VirtualCameraPreparer::~VirtualCameraPreparer()
{
    d->requests.cancelAllRequests();
}

void VirtualCameraPreparer::prepareUploads(const QStringList& filePaths, const QnTimePeriodList& queue)
{
    NX_ASSERT(d->upload.elements.isEmpty());
    NX_ASSERT(!filePaths.isEmpty());
    if (!NX_ASSERT(connection()))
        return;

    for (const QString& path : filePaths)
    {
        VirtualCameraPayload payload;
        payload.path = path;
        d->upload.elements.push_back(payload);
    }

    d->queuePeriods = queue;

    for (VirtualCameraPayload& payload: d->upload.elements)
        checkLocally(payload);

    if (!d->upload.someHaveStatus(VirtualCameraPayload::Valid))
    {
        emit finishedLater(d->upload);
        return;
    }

    QnVirtualCameraPrepareData request;
    for(int i = 0; i < d->upload.elements.size(); i++)
    {
        VirtualCameraPayload& payload = d->upload.elements[i];

        if (payload.status == VirtualCameraPayload::Valid)
        {
            d->localByRemoteIndex.push_back(i);
            request.elements.push_back(payload.local);
        }
    }

    const auto callback = nx::utils::guarded(this,
        [this](bool success, rest::Handle handle, const nx::network::rest::JsonResult& result)
        {
            d->requests.releaseHandle(handle);
            QnVirtualCameraPrepareReply reply = result.deserialized<QnVirtualCameraPrepareReply>();
            handlePrepareFinished(success, reply);
        });

    d->requests.storeHandle(connectedServerApi()->prepareVirtualCameraUploads(
        d->camera,
        request,
        callback,
        thread()));
}

void VirtualCameraPreparer::checkLocally(VirtualCameraPayload& payload)
{
    if (!QFile::exists(payload.path))
    {
        payload.status = VirtualCameraPayload::FileDoesntExist;
        return;
    }

    QnAviResourcePtr resource(new QnAviResource(payload.path));
    QnAviArchiveDelegatePtr delegate(resource->createArchiveDelegate());
    bool opened = delegate->open(resource);
    if (!opened || !delegate->hasVideo() || resource->hasFlags(Qn::still_image))
    {
        payload.status = VirtualCameraPayload::UnsupportedFormat;
        return;
    }

    if (delegate->endTime() == AV_NOPTS_VALUE)
    {
        payload.status = VirtualCameraPayload::NoTimestamp;
        return;
    }
    qint64 durationMs = (delegate->endTime() - delegate->startTime()) / 1000;
    qint64 startTimeMs = delegate->startTime() / 1000;
    qint64 timeOffsetMs = 0;

    if (startTimeMs == 0)
    {
        // correct creation time for iPhone raw files
        QString startTimeString = delegate->getTagValue("com.apple.quicktime.creationdate");
        if (startTimeString.isEmpty())
        {
            startTimeString = delegate->getTagValue("creation_time");
            QString androidVersion = delegate->getTagValue("com.android.version");
            if (!androidVersion.isEmpty())
            {
                // Android mp4 muxer sets creation time at the end of video recording, so we need to
                // subtract duration to get start time.
                timeOffsetMs = durationMs;
            }
        }

        QDateTime startDateTime = QDateTime::fromString(startTimeString, Qt::ISODate);
        if (startDateTime.isValid())
        {
            bool ignoreTimeZone = false;
            if (auto virtualCamera = d->camera.dynamicCast<QnVirtualCameraResource>())
                ignoreTimeZone = virtualCamera->virtualCameraIgnoreTimeZone();

            if (ignoreTimeZone)
            {
                // Interpret UTC time from the file as local time, so reduce it by the local time offset
                const QDateTime now = QDateTime::currentDateTime();
                timeOffsetMs += now.offsetFromUtc() * 1000;
            }

            startTimeMs = startDateTime.toMSecsSinceEpoch() - timeOffsetMs;
        }
    }

    if (startTimeMs == 0)
    {
        payload.status = VirtualCameraPayload::NoTimestamp;
        return;
    }

    if (durationMs == 0)
    {
        payload.status = VirtualCameraPayload::NoTimestamp;
        return;
    }

    QnTimePeriod localPeriod = QnTimePeriod(startTimeMs, durationMs);
    payload.local.period = localPeriod;

    if (d->camera->maxPeriod().count() > 0)
    {
        const auto minTime = qnSyncTime->currentTimePoint() - d->camera->maxPeriod();
        const qint64 minTimeMs = duration_cast<std::chrono::milliseconds>(minTime).count();
        if (startTimeMs < minTimeMs)
        {
            payload.status = VirtualCameraPayload::FootagePastMaxDays;
            return;
        }
    }

    QnTimePeriod expectedRemotePeriod = shrinkPeriod(localPeriod, d->queuePeriods);
    if (!canUpload(localPeriod, expectedRemotePeriod))
    {
        payload.status = VirtualCameraPayload::ChunksTakenByFileInQueue;
        return;
    }

    payload.local.size = QFileInfo(payload.path).size();
    payload.status = VirtualCameraPayload::Valid;

    d->queuePeriods.includeTimePeriod(payload.local.period);
}

void VirtualCameraPreparer::handlePrepareFinished(bool success, const QnVirtualCameraPrepareReply& reply)
{
    if (!success || reply.elements.size() != d->localByRemoteIndex.size())
    {
        for (VirtualCameraPayload& payload: d->upload.elements)
            payload.status = VirtualCameraPayload::ServerError;

        emit finished(d->upload);
        return;
    }

    for (int i = 0; i < reply.elements.size(); i++)
    {
        VirtualCameraPayload& payload = d->upload.elements[d->localByRemoteIndex[i]];
        payload.remote = reply.elements[i];
        if (!canUpload(payload.local.period, payload.remote.period))
            payload.status = VirtualCameraPayload::ChunksTakenOnServer;
    }

    if (reply.storageCleanupNeeded || reply.storageFull)
    {
        VirtualCameraPayload::Status newStatus = reply.storageFull
            ? VirtualCameraPayload::NoSpaceOnServer
            : VirtualCameraPayload::StorageCleanupNeeded;
        for (VirtualCameraPayload& payload: d->upload.elements)
            if (payload.status == VirtualCameraPayload::Valid)
                payload.status = newStatus;
    }

    emit finished(d->upload);
}

} // namespace nx::vms::client::desktop
