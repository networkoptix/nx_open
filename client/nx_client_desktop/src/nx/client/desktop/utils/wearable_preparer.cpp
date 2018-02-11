#include "wearable_preparer.h"

#include <QtCore/QFile>
#include <QtCore/QFileInfo>

#include <utils/common/guarded_callback.h>
#include <api/server_rest_connection.h>
#include <core/resource/security_cam_resource.h>
#include <core/resource/media_server_resource.h>
#include <recording/time_period_list.h>

#include <plugins/resource/avi/avi_resource.h>
#include <plugins/resource/avi/avi_archive_delegate.h>

#include "server_request_storage.h"

namespace nx {
namespace client {
namespace desktop {

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


struct WearablePreparer::Private
{
    Private(const QnSecurityCamResourcePtr& camera) :
        camera(camera),
        server(camera->getParentServer()),
        requests(camera->getParentServer())
    {
    }

    rest::QnConnectionPtr connection() const
    {
        return server->restConnection();
    }

    QnSecurityCamResourcePtr camera;
    QnMediaServerResourcePtr server;
    QnTimePeriodList queuePeriods;
    WearableUpload upload;
    QVector<int> localByRemoteIndex;

    ServerRequestStorage requests;
};

WearablePreparer::WearablePreparer(const QnSecurityCamResourcePtr& camera, QObject* parent):
    base_type(parent),
    QnCommonModuleAware(parent),
    d(new Private(camera))
{
    connect(this, &WearablePreparer::finishedLater, this,
        &WearablePreparer::finished, Qt::QueuedConnection);
}

WearablePreparer::~WearablePreparer()
{
    d->requests.cancelAllRequests();
}

void WearablePreparer::prepareUploads(const QStringList& filePaths, const QnTimePeriodList& queue)
{
    NX_ASSERT(d->upload.elements.isEmpty());
    NX_ASSERT(!filePaths.isEmpty());

    for (const QString& path : filePaths)
    {
        WearablePayload payload;
        payload.path = path;
        d->upload.elements.push_back(payload);
    }

    d->queuePeriods = queue;

    for (WearablePayload& payload : d->upload.elements)
        checkLocally(payload);

    if (!d->upload.someHaveStatus(WearablePayload::Valid))
    {
        emit finishedLater(d->upload);
        return;
    }

    QnWearableCheckData request;
    for(int i = 0; i < d->upload.elements.size(); i++)
    {
        WearablePayload& payload = d->upload.elements[i];

        if (payload.status == WearablePayload::Valid)
        {
            d->localByRemoteIndex.push_back(i);
            request.elements.push_back(payload.local);
        }
    }

    auto callback = guarded(this,
        [this](bool success, rest::Handle handle, const QnJsonRestResult& result)
        {
            d->requests.releaseHandle(handle);
            QnWearableCheckReply reply = result.deserialized<QnWearableCheckReply>();
            handlePrepareFinished(success, reply);
        });

    d->requests.storeHandle(d->connection()->prepareWearableUploads(d->camera, request, callback, thread()));
}

void WearablePreparer::checkLocally(WearablePayload& payload)
{
    if (!QFile::exists(payload.path))
    {
        payload.status = WearablePayload::FileDoesntExist;
        return;
    }

    QnAviResourcePtr resource(new QnAviResource(payload.path));
    QnAviArchiveDelegatePtr delegate(resource->createArchiveDelegate());
    bool opened = delegate->open(resource) && delegate->findStreams();

    if (!opened || !delegate->hasVideo() || resource->hasFlags(Qn::still_image))
    {
        payload.status = WearablePayload::UnsupportedFormat;
        return;
    }

    qint64 startTimeMs = delegate->startTime() / 1000;

    if (startTimeMs == 0)
    {
        QString startTimeString = QLatin1String(delegate->getTagValue("creation_time"));
        QDateTime startDateTime = QDateTime::fromString(startTimeString, Qt::ISODate);
        if (startDateTime.isValid())
            startTimeMs = startDateTime.toMSecsSinceEpoch();
    }

    if (startTimeMs == 0)
    {
        payload.status = WearablePayload::NoTimestamp;
        return;
    }

    qint64 durationMs = (delegate->endTime() - delegate->startTime()) / 1000;

    if (durationMs == 0)
    {
        payload.status = WearablePayload::NoTimestamp;
        return;
    }

    QnTimePeriod localPeriod = QnTimePeriod(startTimeMs, durationMs);
    QnTimePeriod expectedRemotePeriod = shrinkPeriod(localPeriod, d->queuePeriods);
    if (!canUpload(localPeriod, expectedRemotePeriod))
    {
        payload.status = WearablePayload::ChunksTakenByFileInQueue;
        return;
    }

    payload.local.period = localPeriod;
    payload.local.size = QFileInfo(payload.path).size();
    payload.status = WearablePayload::Valid;

    d->queuePeriods.includeTimePeriod(payload.local.period);
}

void WearablePreparer::handlePrepareFinished(bool success, const QnWearableCheckReply& reply)
{
    if (!success || reply.elements.size() != d->localByRemoteIndex.size())
    {
        for (WearablePayload& payload : d->upload.elements)
            payload.status = WearablePayload::ServerError;

        emit finished(d->upload);
        return;
    }

    qint64 totalSize = 0;
    for (const WearablePayload& payload : d->upload.elements)
        if (payload.status == WearablePayload::Valid)
            totalSize += payload.local.size;

    d->upload.spaceRequested = totalSize;
    d->upload.spaceAvailable = reply.availableSpace;

    if (totalSize > reply.availableSpace)
    {
        for (WearablePayload& payload : d->upload.elements)
            if (payload.status == WearablePayload::Valid)
                payload.status = WearablePayload::NoSpaceOnServer;
    }
    else
    {
        for (int i = 0; i < reply.elements.size(); i++)
        {
            WearablePayload& payload = d->upload.elements[d->localByRemoteIndex[i]];
            payload.remote = reply.elements[i];
            if (!canUpload(payload.local.period, payload.remote.period))
                payload.status = WearablePayload::ChunksTakenOnServer;
        }
    }

    emit finished(d->upload);
}

} // namespace desktop
} // namespace client
} // namespace nx


