#include "export_layout_tool.h"

#include <QtCore/QBuffer>
#include <QtCore/QEventLoop>

#include <common/common_module.h>

#include <client_core/client_core_module.h>

#include <client/client_settings.h>
#include <client/client_module.h>

#include <camera/camera_data_manager.h>
#include <camera/loaders/caching_camera_data_loader.h>
#include <camera/client_video_camera.h>

#include <core/resource/resource.h>
#include <core/resource/resource_display_info.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource/media_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>

#include <nx_ec/data/api_layout_data.h>
#include <nx_ec/data/api_conversion_functions.h>

#include <plugins/storage/file_storage/layout_storage_resource.h>
#include <plugins/resource/avi/avi_resource.h>

#include <ui/workbench/workbench_layout_snapshot_manager.h>

#include <nx/fusion/model_functions.h>
#include <nx/client/desktop/utils/server_image_cache.h>
#include <nx/client/desktop/utils/local_file_cache.h>

#include <nx/utils/app_info.h>
#include "ui/workbench/watchers/workbench_server_time_watcher.h"

#ifdef Q_OS_WIN
#   include <launcher/nov_launcher_win.h>
#endif

namespace {

const int kFileOperationRetries = 3;
const int kFileOperationRetryDelayMs = 500;

} // namespace

namespace nx {
namespace client {
namespace desktop {

struct ExportLayoutTool::Private
{
    ExportLayoutTool* const q;
    const ExportLayoutSettings settings;
    ExportProcessError lastError = ExportProcessError::noError;
    ExportProcessStatus status = ExportProcessStatus::initial;

    // Queue of resources to export.
    QQueue<QnMediaResourcePtr> resources;

    // File that is actually written. May differ from target to avoid antivirus exe checks.
    QString actualFilename;

    // External file storage.
    QnStorageResourcePtr storage;

    // Number of exported cameras with archive.
    int camerasWithData = 0;

    explicit Private(ExportLayoutTool* owner, const ExportLayoutSettings& settings):
        q(owner),
        settings(settings),
        actualFilename(settings.filename.completeFileName())
    {
    }

    void cancelExport()
    {
        NX_ASSERT(status == ExportProcessStatus::exporting);
        setStatus(ExportProcessStatus::cancelling);
        resources.clear();

    }

    void setStatus(ExportProcessStatus value)
    {
        NX_EXPECT(status != value);
        status = value;
        emit q->statusChanged(status);
    }

    void finishExport()
    {
        switch (status)
        {
            case ExportProcessStatus::exporting:
                setStatus(lastError == ExportProcessError::noError
                    ? ExportProcessStatus::success
                    : ExportProcessStatus::failure);
                break;
            case ExportProcessStatus::cancelling:
                setStatus(ExportProcessStatus::cancelled);
                break;
            default:
                NX_ASSERT(false, "Should never get here");
                break;
        }
        if (status != ExportProcessStatus::success)
        {
            QFile::remove(actualFilename);
        }
        else
        {
            const auto targetFilename = settings.filename.completeFileName();
            if (actualFilename != targetFilename)
                storage->renameFile(storage->getUrl(), targetFilename);
        }
        emit q->finished();
    }
};

ExportLayoutTool::ItemInfo::ItemInfo():
    name(),
    timezone(0)
{
}

ExportLayoutTool::ItemInfo::ItemInfo(const QString& name, qint64 timezone):
    name(name),
    timezone(timezone)
{
}

ExportLayoutTool::ExportLayoutTool(ExportLayoutSettings settings, QObject* parent):
    base_type(parent),
    d(new Private(this, settings))
{
    m_layout.reset(new QnLayoutResource());
    m_layout->setId(d->settings.layout->getId()); //before update() uuid's must be the same
    m_layout->update(d->settings.layout);

    // If exporting layout, create new guid. If layout just renamed, keep guid
    if (d->settings.mode != ExportLayoutSettings::Mode::LocalSave)
        m_layout->setId(QnUuid::createUuid());
}

ExportLayoutTool::~ExportLayoutTool()
{
}

bool ExportLayoutTool::prepareStorage()
{
    const auto isExeFile = utils::AppInfo::isWindows()
        && FileExtensionUtils::isExecutable(d->settings.filename.extension);

    if (isExeFile || d->actualFilename == m_layout->getUrl())
    {
        // can not override opened layout. save to tmp file, then rename
        d->actualFilename += lit(".tmp");
    }

#ifdef Q_OS_WIN
    if (isExeFile)
    {
        // TODO: #GDM handle other errors
        if (QnNovLauncher::createLaunchingFile(d->actualFilename) != QnNovLauncher::ErrorCode::Ok)
            return false;
    }
    else
#endif
    {
        QFile::remove(d->actualFilename);
    }

    d->storage = QnStorageResourcePtr(new QnLayoutFileStorageResource(d->settings.layout->commonModule()));
    d->storage->setUrl(d->actualFilename);
    return true;
}

ExportLayoutTool::ItemInfoList ExportLayoutTool::prepareLayout()
{
    ItemInfoList result;

    QSet<QString> uniqIdList;
    QnLayoutItemDataMap items;

    // Take resource pool from the original layout.
    auto resourcePool = d->settings.layout->resourcePool();
    if (!resourcePool)
        resourcePool = qnClientCoreModule->commonModule()->resourcePool();

    for (const auto& item: m_layout->getItems())
    {
        const auto resource = resourcePool->getResourceByDescriptor(item.resource);
        const bool skip = mode() == ExportLayoutSettings::Mode::Export
            ? !resource.dynamicCast<QnVirtualCameraResource>()
            : (resource->hasFlags(Qn::server) || resource->hasFlags(Qn::web_page));

        if (skip)
            continue;

        QnLayoutItemData localItem = item;

        ItemInfo info(resource->getName(), Qn::InvalidUtcOffset);

        if (QnMediaResourcePtr mediaRes = resource.dynamicCast<QnMediaResource>())
        {
            QString uniqueId = resource->getUniqueId();
            localItem.resource.id = resource->getId();
            localItem.resource.uniqueId = uniqueId;
            if (!uniqIdList.contains(uniqueId))
            {
                d->resources << mediaRes;
                uniqIdList << uniqueId;
            }
            info.timezone = QnWorkbenchServerTimeWatcher::utcOffset(mediaRes, Qn::InvalidUtcOffset);
        }

        items.insert(localItem.uuid, localItem);
        result.append(info);
    }
    m_layout->setItems(items);
    return result;
}


bool ExportLayoutTool::exportMetadata(const ItemInfoList &items)
{

    /* Names of exported resources. */
    {
        QByteArray names;
        QTextStream itemNames(&names, QIODevice::WriteOnly);
        for (const ItemInfo &info : items)
            itemNames << info.name << lit("\n");
        itemNames.flush();

        if (!writeData(lit("item_names.txt"), names))
            return false;
    }

    /* Timezones of exported resources. */
    {
        QByteArray timezones;
        QTextStream itemTimeZonesStream(&timezones, QIODevice::WriteOnly);
        for (const ItemInfo &info : items)
            itemTimeZonesStream << info.timezone << lit("\n");
        itemTimeZonesStream.flush();

        if (!writeData(lit("item_timezones.txt"), timezones))
            return false;
    }

    /* Layout items. */
    {
        QByteArray layoutData;
        ec2::ApiLayoutData layoutObject;
        fromResourceToApi(m_layout, layoutObject);
        QJson::serialize(layoutObject, &layoutData);
        /* Old name for compatibility issues. */
        if (!writeData(lit("layout.pb"), layoutData))
            return false;
    }

    /* Exported period. */
    {
        if (!writeData(lit("range.bin"), d->settings.period.serialize()))
            return false;
    }

    /* Additional flags. */
    {
        quint32 flags = d->settings.readOnly ? QnLayoutFileStorageResource::ReadOnly : 0;
        for (const QnMediaResourcePtr resource : d->resources)
        {
            if (resource->toResource()->hasFlags(Qn::utc))
            {
                flags |= QnLayoutFileStorageResource::ContainsCameras;
                break;
            }
        }

        if (!tryInLoop([this, flags]
        {
            QScopedPointer<QIODevice> miscFile(d->storage->open(lit("misc.bin"), QIODevice::WriteOnly));
            if (!miscFile)
                return false;
            miscFile->write((const char*)&flags, sizeof(flags));
            return true;
        }))
            return false;
    }

    /* Layout id. */
    {
        if (!writeData(lit("uuid.bin"), m_layout->getId().toByteArray()))
            return false;
    }

    /* Chunks. */
    for (const QnMediaResourcePtr &resource : d->resources)
    {
        QString uniqId = resource->toResource()->getUniqueId();
        uniqId = uniqId.mid(uniqId.lastIndexOf(L'?') + 1);
        auto loader = qnClientModule->cameraDataManager()->loader(resource);
        if (!loader)
            continue;
        QnTimePeriodList periods = loader->periods(Qn::RecordingContent).intersected(d->settings.period);
        QByteArray data;
        periods.encode(data);

        if (!writeData(lit("chunk_%1.bin").arg(QFileInfo(uniqId).completeBaseName()), data))
            return false;
    }

    /* Layout background */
    if (!m_layout->backgroundImageFilename().isEmpty())
    {
        bool exportedLayout = m_layout->isFile();  // we have changed background to an exported layout
        QScopedPointer<ServerImageCache> cache;
        if (exportedLayout)
            cache.reset(new LocalFileCache(this));
        else
            cache.reset(new ServerImageCache(this));

        QImage background(cache->getFullPath(m_layout->backgroundImageFilename()));
        if (!background.isNull())
        {

            if (!tryInLoop([this, background]
            {
                QScopedPointer<QIODevice> imageFile(d->storage->open(m_layout->backgroundImageFilename(), QIODevice::WriteOnly));
                if (!imageFile)
                    return false;
                background.save(imageFile.data(), "png");
                return true;
            }))
                return false;

            LocalFileCache localCache;
            localCache.storeImageData(m_layout->backgroundImageFilename(), background);
        }
    }

    return true;
}

bool ExportLayoutTool::start()
{
    d->camerasWithData = 0;

    if (!prepareStorage())
    {
        d->lastError = ExportProcessError::fileAccess;
        d->setStatus(ExportProcessStatus::failure);
        return false;
    }

    ItemInfoList items = prepareLayout();

    if (!exportMetadata(items))
    {
        d->lastError = ExportProcessError::fileAccess;
        d->setStatus(ExportProcessStatus::failure);
        return false;
    }

    emit rangeChanged(0, d->resources.size() * 100);
    emit valueChanged(0);
    d->setStatus(ExportProcessStatus::exporting);
    return exportNextCamera();
}

void ExportLayoutTool::stop()
{
    d->cancelExport();

    if (m_currentCamera)
    {
        connect(m_currentCamera, SIGNAL(exportStopped()), this, SLOT(at_camera_exportStopped()));
        m_currentCamera->stopExport();
    }
    else
    {
        finishExport(false);
    }
}

ExportLayoutSettings::Mode ExportLayoutTool::mode() const
{
    return d->settings.mode;
}

ExportProcessError ExportLayoutTool::lastError() const
{
    return d->lastError;
}

ExportProcessStatus ExportLayoutTool::processStatus() const
{
    return d->status;
}

bool ExportLayoutTool::exportNextCamera()
{
    if (d->status != ExportProcessStatus::exporting)
        return false;

    if (d->resources.isEmpty())
    {
        if (d->camerasWithData)
        {
            finishExport(true);
        }
        else
        {
            d->lastError = ExportProcessError::dataNotFound;
            finishExport(false);
        }
        return false;
    }

    m_offset++;
    return exportMediaResource(d->resources.dequeue());
}

void ExportLayoutTool::finishExport(bool success)
{
    d->finishExport();

    // TODO: #GDM This is more correct "Save as" handling, but it does not work yet.
    /*
    else if (m_mode == Qn::LayoutLocalSaveAs)
    {
        QString oldUrl = m_layout->getUrl();
        QString newUrl = d->storage->getUrl();

        for (const QnLayoutItemData &item : m_layout->getItems())
        {
            QnAviResourcePtr aviRes = resourcePool()->getResourceByUniqueId<QnAviResource>(item.resource.uniqueId);
            if (aviRes)
            {
                aviRes->setUniqueId(QnLayoutFileStorageResource::itemUniqueId(newUrl,
                    item.resource.uniqueId));
            }
        }
        m_layout->setUrl(newUrl);
        m_layout->setName(QFileInfo(newUrl).fileName());

        QnLayoutFileStorageResourcePtr novStorage = d->storage.dynamicCast<QnLayoutFileStorageResource>();
        if (novStorage)
            novStorage->switchToFile(oldUrl, newUrl, false);
        snapshotManager()->store(m_layout);
    }
    */

}

bool ExportLayoutTool::exportMediaResource(const QnMediaResourcePtr& resource)
{
    m_currentCamera = new QnClientVideoCamera(resource);
    connect(m_currentCamera, SIGNAL(exportProgress(int)), this, SLOT(at_camera_progressChanged(int)));
    connect(m_currentCamera, &QnClientVideoCamera::exportFinished, this,
        &ExportLayoutTool::at_camera_exportFinished);

    int numberOfChannels = resource->getVideoLayout()->channelCount();
    for (int i = 0; i < numberOfChannels; ++i)
    {
        QSharedPointer<QBuffer> motionFileBuffer(new QBuffer());
        motionFileBuffer->open(QIODevice::ReadWrite);
        m_currentCamera->setMotionIODevice(motionFileBuffer, i);
    }

    QString uniqId = resource->toResource()->getUniqueId();
    uniqId = uniqId.mid(uniqId.indexOf(L'?') + 1); // simplify name if export from existing layout
    auto role = StreamRecorderRole::fileExport;
    if (resource->toResource()->hasFlags(Qn::utc))
        role = StreamRecorderRole::fileExportWithEmptyContext;

    qint64 serverTimeZone = QnWorkbenchServerTimeWatcher::utcOffset(resource, Qn::InvalidUtcOffset);

    m_currentCamera->exportMediaPeriodToFile(d->settings.period,
        uniqId,
        lit("mkv"),
        d->storage,
        role,
        serverTimeZone,
        0,
        nx::core::transcoding::FilterChain());

    return true;
}

void ExportLayoutTool::at_camera_exportFinished(const StreamRecorderErrorStruct& status,
    const QString& /*filename*/)
{
    d->lastError = convertError(status.lastError);
    if (d->lastError == ExportProcessError::dataNotFound)
    {
        d->lastError = ExportProcessError::noError;
        exportNextCamera();
        return;
    }

    ++d->camerasWithData;

    if (d->lastError != ExportProcessError::noError)
    {
        finishExport(false);
        return;
    }

    auto camera = dynamic_cast<QnClientVideoCamera*>(sender());
    if (!camera)
        return;

    int numberOfChannels = camera->resource()->getVideoLayout()->channelCount();
    bool error = false;

    for (int i = 0; i < numberOfChannels; ++i)
    {
        if (QSharedPointer<QBuffer> motionFileBuffer = camera->motionIODevice(i))
        {
            motionFileBuffer->close();
            if (error)
                continue;

            QString uniqId = camera->resource()->toResource()->getUniqueId();
            uniqId = QFileInfo(uniqId.mid(uniqId.indexOf(L'?') + 1)).completeBaseName(); // simplify name if export from existing layout
            QString motionFileName = lit("motion%1_%2.bin").arg(i).arg(uniqId);
            /* Other buffers must be closed even in case of error. */
            writeData(motionFileName, motionFileBuffer->buffer());
        }
    }

    camera->deleteLater();

    if (error)
    {
        auto camRes = camera->resource()->toResourcePtr().dynamicCast<QnVirtualCameraResource>();
        NX_ASSERT(camRes, Q_FUNC_INFO, "Make sure camera exists");
        const auto resourcePool = camRes->resourcePool();
        NX_ASSERT(resourcePool);
        d->lastError = ExportProcessError::unsupportedMedia;
        finishExport(false);
    }
    else
    {
        exportNextCamera();
    }
}

void ExportLayoutTool::at_camera_exportStopped()
{
    if (m_currentCamera)
        m_currentCamera->deleteLater();

    finishExport(false);
}

void ExportLayoutTool::at_camera_progressChanged(int progress)
{
    emit valueChanged(m_offset * 100 + progress);
}

bool ExportLayoutTool::tryInLoop(std::function<bool()> handler)
{
    if (!handler)
        return false;

    QPointer<QObject> guard(this);
    int times = kFileOperationRetries;

    while (times > 0)
    {
        if (!guard)
            return false;

        if (handler())
            return true;

        times--;
        if (times <= 0)
            return false;

        QScopedPointer<QTimer> timer(new QTimer());
        timer->setSingleShot(true);
        timer->setInterval(kFileOperationRetryDelayMs);

        QScopedPointer<QEventLoop> loop(new QEventLoop());
        connect(timer.data(), &QTimer::timeout, loop.data(), &QEventLoop::quit);
        timer->start();
        loop->exec();
    }

    return false;
}

bool ExportLayoutTool::writeData(const QString &fileName, const QByteArray &data)
{
    return tryInLoop(
        [this, fileName, data]
        {
            QScopedPointer<QIODevice> file(d->storage->open(fileName, QIODevice::WriteOnly));
            if (!file)
                return false;
            file->write(data);
            return true;
        });
}

} // namespace desktop
} // namespace client
} // namespace nx
