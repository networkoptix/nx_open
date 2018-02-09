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
}

WearableChecker::~WearableChecker()
{
    d->requests.cancelAllRequests();
}

WearablePayloadList WearableChecker::checkUploads(const QStringList& filePaths)
{
    NX_ASSERT(d->result.isEmpty());
    NX_ASSERT(!filePaths.isEmpty());

    for (const QString& path : filePaths)
        checkLocally(path);

    return d->result;
}

void WearableChecker::checkLocally(const QString& filePath)
{
    d->result.push_back(WearablePayload());
    WearablePayload& payload = d->result.back();
    payload.path = filePath;

    if (!QFile::exists(filePath))
    {
        payload.status = WearablePayload::FileDoesntExist;
        return;
    }

    QnAviResourcePtr resource(new QnAviResource(filePath));
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

    payload.startTimeMs = startTimeMs;
    payload.durationMs = durationMs;
    payload.size = QFileInfo(filePath).size();
    payload.status = WearablePayload::Valid;
}

} // namespace desktop
} // namespace client
} // namespace nx


