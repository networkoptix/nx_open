#include "wearable_checker.h"

#include <QtCore/QFile>
#include <QtCore/QFileInfo>

#include <api/server_rest_connection.h>
#include <core/resource/security_cam_resource.h>
#include <core/resource/media_server_resource.h>

#include <plugins/resource/avi/avi_resource.h>
#include <plugins/resource/avi/avi_archive_delegate.h>

#include "server_request_storage.h"

namespace nx {
namespace client {
namespace desktop {

namespace
{
    qint64 kMaxIgnoredOverlapMs = 200;
}


struct WearableChecker::Private
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
    WearablePayloadList result;

    ServerRequestStorage requests;
};

WearableChecker::WearableChecker(const QnSecurityCamResourcePtr& camera, QObject* parent):
    base_type(parent),
    QnCommonModuleAware(parent),
    d(new Private(camera))
{
    connect(this, &WearableChecker::finishedLater, this,
        &WearableChecker::finished, Qt::QueuedConnection);
}

WearableChecker::~WearableChecker()
{
    d->requests.cancelAllRequests();
}

void WearableChecker::checkUploads(const QStringList& filePaths)
{
    NX_ASSERT(d->result.isEmpty());
    NX_ASSERT(!filePaths.isEmpty());

    for (const QString& path : filePaths)
    {
        WearablePayload payload;
        payload.path = path;
        d->result.push_back(payload);
    }

    for (WearablePayload& payload : d->result)
        checkLocally(payload);

    if (!WearablePayload::someHaveStatus(d->result, WearablePayload::Valid))
    {
        emit finishedLater(d->result);
        return;
    }

    QnWearableCheckData request;
    for (const WearablePayload& payload : d->result)
        request.elements.push_back(payload.local);

    auto callback =
        [this](bool success, rest::Handle handle, const QnJsonRestResult& result)
        {
            d->requests.releaseHandle(handle);
            QnWearableCheckReply reply = result.deserialized<QnWearableCheckReply>();
            handleCheckFinished(success, reply);
        };

    d->requests.storeHandle(d->connection()->checkWearableUploads(d->camera, request, callback, thread()));
}

void WearableChecker::checkLocally(WearablePayload& payload)
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

    payload.local.period = QnTimePeriod(startTimeMs, durationMs);
    payload.local.size = QFileInfo(payload.path).size();
    payload.status = WearablePayload::Valid;
}

void WearableChecker::handleCheckFinished(bool success, const QnWearableCheckReply& reply)
{
    if (!success || reply.elements.size() != d->result.size())
    {
        for (WearablePayload& payload : d->result)
            payload.status = WearablePayload::ServerError;

        emit finished(d->result);
        return;
    }

    for (int i = 0; i < reply.elements.size(); i++)
    {
        WearablePayload& payload = d->result[i];
        payload.remote = reply.elements[i];

        if (payload.local.period != payload.remote.period)
        {
            QnTimePeriod remoteExtended = payload.remote.period;
            remoteExtended.startTimeMs -= kMaxIgnoredOverlapMs;
            remoteExtended.durationMs += 2 * kMaxIgnoredOverlapMs;

            if (!remoteExtended.contains(payload.local.period))
                payload.status = WearablePayload::ChunksTakenOnServer;
        }
    }

    emit finished(d->result);
}

} // namespace desktop
} // namespace client
} // namespace nx


