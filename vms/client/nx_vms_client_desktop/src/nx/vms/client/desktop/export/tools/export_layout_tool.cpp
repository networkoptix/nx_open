// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "export_layout_tool.h"

#include <QtCore/QBuffer>
#include <QtCore/QEventLoop>

#include <camera/camera_data_manager.h>
#include <camera/client_video_camera.h>
#include <client/client_settings.h>
#include <core/resource/avi/avi_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/file_layout_resource.h>
#include <core/resource/layout_reader.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_resource.h>
#include <core/resource/resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/storage/file_storage/layout_storage_resource.h>
#include <nx/build_info.h>
#include <nx/core/watermark/watermark.h>
#include <nx/fusion/model_functions.h>
#include <nx/vms/api/data/layout_data.h>
#include <nx/vms/client/core/resource/data_loaders/caching_camera_data_loader.h>
#include <nx/vms/client/core/watchers/server_time_watcher.h>
#include <nx/vms/client/desktop/resource/layout_password_management.h>
#include <nx/vms/client/desktop/resource/resource_descriptor.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/utils/local_file_cache.h>
#include <nx/vms/client/desktop/utils/server_image_cache.h>
#include <nx_ec/data/api_conversion_functions.h>

#ifdef Q_OS_WIN
    #include <launcher/nov_launcher_win.h>
#endif

namespace {

const int kFileOperationRetries = 3;
const int kFileOperationRetryDelayMs = 500;

QString fileNameForResource(const QnResourcePtr& resource)
{
    QString uniqId;
    if (auto camera = resource.dynamicCast<QnSecurityCamResource>())
        uniqId = camera->getPhysicalId();
    else
        uniqId = resource->getUrl();
    uniqId = QFileInfo(uniqId.mid(uniqId.indexOf('?') + 1)).completeBaseName(); // simplify name if export from existing layout
    return uniqId;
}

} // namespace

namespace nx::vms::client::desktop {

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

    // Whether any archive has been exported.
    bool exportedAnyData = false;

    /** Source layout. */
    QnLayoutResourcePtr originalLayout;

    /** Copy of the provided layout. */
    QnLayoutResourcePtr layout;

    explicit Private(ExportLayoutTool* owner, const ExportLayoutSettings& settings):
        q(owner),
        settings(settings),
        actualFilename(settings.fileName.completeFileName())
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
        NX_ASSERT(status != value);
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
            const auto targetFilename = settings.fileName.completeFileName();
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

ExportLayoutTool::ExportLayoutTool(
    ExportLayoutSettings settings,
    QnLayoutResourcePtr layout,
    QObject* parent)
    :
    base_type(parent),
    d(new Private(this, settings))
{
    d->originalLayout = layout;
    // Make d->layout a deep exact copy of original layout.
    d->layout.reset(settings.mode == ExportLayoutSettings::Mode::LocalSave
        ? new QnFileLayoutResource()
        : new QnLayoutResource());
    d->layout->setIdUnsafe(layout->getId()); //before update() uuid's must be the same
    d->layout->update(layout);

    // If exporting layout, create new guid. If layout just renamed or saved, keep guid.
    if (d->settings.mode != ExportLayoutSettings::Mode::LocalSave)
        d->layout->setIdUnsafe(QnUuid::createUuid());

    m_isExportToExe = nx::build_info::isWindows()
        && FileExtensionUtils::isExecutable(d->settings.fileName.extension);
}

ExportLayoutTool::~ExportLayoutTool()
{
}

bool ExportLayoutTool::prepareStorage()
{
    // Save to tmp file, then rename.
    d->actualFilename += lit(".tmp");

#ifdef Q_OS_WIN
    if (m_isExportToExe)
    {
        // TODO: #sivanov handle other errors
        if (QnNovLauncher::createLaunchingFile(d->actualFilename) != QnNovLauncher::ErrorCode::Ok)
            return false;
    }
    else
#endif
    {
        QFile::remove(d->actualFilename);
    }

    auto fileStorage = new QnLayoutFileStorageResource(d->actualFilename);

    if (d->settings.encryption.on)
        fileStorage->setPasswordToWrite(d->settings.encryption.password);
    d->storage = QnStorageResourcePtr(fileStorage);
    return true;
}

ExportLayoutTool::ItemInfoList ExportLayoutTool::prepareLayout()
{
    ItemInfoList result;

    QSet<QnUuid> idList;
    QnLayoutItemDataMap items;

    for (const auto& item: d->layout->getItems())
    {
        const auto resource = getResourceByDescriptor(item.resource);
        const auto mediaResource = resource.dynamicCast<QnMediaResource>();
        // We only export video files or cameras; no still images, web pages etc.
        const bool skip = !mediaResource || resource->hasFlags(Qn::still_image);

        if (skip)
            continue;

        const auto id = resource->getId();
        // Only export every resource once, even if it placed on layout several times.
        if (!idList.contains(id))
        {
            d->resources << mediaResource;
            idList << id;
        }

        QnLayoutItemData localItem = item;
        localItem.resource.id = resource->getId();
        localItem.resource.path = fileNameForResource(resource);
        items.insert(localItem.uuid, localItem);

        ItemInfo info(resource->getName(), Qn::InvalidUtcOffset);
        info.timezone = nx::vms::client::core::ServerTimeWatcher::utcOffset(mediaResource,
            Qn::InvalidUtcOffset);
        result.append(info);
    }
    d->layout->setItems(items);

    // Set up layout watermark, but only if it misses one.
    if (d->layout->data(Qn::LayoutWatermarkRole).isNull() && d->settings.watermark.visible())
        d->layout->setData(Qn::LayoutWatermarkRole, QVariant::fromValue(d->settings.watermark));

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
        nx::vms::api::LayoutData layoutObject;
        ec2::fromResourceToApi(d->layout, layoutObject);
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
        if (!writeData(lit("uuid.bin"), d->layout->getId().toByteArray()))
            return false;
    }

    /* Chunks. */
    for (const auto& resource: d->resources)
    {
        QByteArray data;
        if (d->settings.bookmarks.empty())
        {
            auto systemContext = SystemContext::fromResource(resource->toResourcePtr());
            if (!NX_ASSERT(systemContext))
                continue;

            auto loader = systemContext->cameraDataManager()->loader(resource);
            if (!loader)
                continue;

            auto periods = loader->periods(Qn::RecordingContent).intersected(d->settings.period);
            periods.encode(data);
        }
        else
        {
            // We may not have loaded chunks when using Bookmarks export for camera, which was not
            // opened yet. This is no important as bookmarks are automatically deleted when archive
            // is not available for the given period.
            QnTimePeriodList periods;
            for (const auto& bookmark: d->settings.bookmarks)
            {
                if (bookmark.cameraId == resource->toResourcePtr()->getId())
                    periods.includeTimePeriod({bookmark.startTimeMs, bookmark.durationMs});
            }
            NX_ASSERT(std::is_sorted(periods.cbegin(), periods.cend()));
            periods.encode(data);
        }


        auto fileName = nx::format("chunk_%1.bin", fileNameForResource(resource->toResourcePtr()));
        if (!writeData(fileName, data))
            return false;
    }

    /* Layout background */
    if (!d->layout->backgroundImageFilename().isEmpty())
    {
        bool exportedLayout = d->layout->isFile();  // we have changed background to an exported layout
        QScopedPointer<ServerImageCache> cache;
        if (exportedLayout)
            cache.reset(new LocalFileCache(this));
        else
            cache.reset(new ServerImageCache(this));

        QImage background(cache->getFullPath(d->layout->backgroundImageFilename()));
        if (!background.isNull())
        {

            if (!tryInLoop([this, background]
            {
                QScopedPointer<QIODevice> imageFile(d->storage->open(d->layout->backgroundImageFilename(), QIODevice::WriteOnly));
                if (!imageFile)
                    return false;
                background.save(imageFile.data(), "png");
                return true;
            }))
                return false;

            LocalFileCache localCache;
            localCache.storeImageData(d->layout->backgroundImageFilename(), background);
        }
    }

    /* Watermark */
    if (d->layout->data(Qn::LayoutWatermarkRole).isValid())
    {
        if (!writeData(lit("watermark.txt"),
            QJson::serialized(d->layout->data(Qn::LayoutWatermarkRole).value<nx::core::Watermark>())))
        {
            return false;
        }
    }

    return true;
}

bool ExportLayoutTool::start()
{
    d->exportedAnyData = false;

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
        m_currentCamera->stopExport();

    finishExport(false);
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
        if (d->exportedAnyData)
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
}

bool ExportLayoutTool::exportMediaResource(const QnMediaResourcePtr& resource)
{
    m_currentCamera.reset(new QnClientVideoCamera(resource));
    connect(m_currentCamera.get(), SIGNAL(exportProgress(int)), this, SLOT(at_camera_progressChanged(int)));
    connect(m_currentCamera.get(), &QnClientVideoCamera::exportFinished, this,
        &ExportLayoutTool::at_camera_exportFinished);

    int numberOfChannels = resource->getVideoLayout()->channelCount();
    for (int i = 0; i < numberOfChannels; ++i)
    {
        QSharedPointer<QBuffer> motionFileBuffer(new QBuffer());
        motionFileBuffer->open(QIODevice::ReadWrite);
        m_currentCamera->setMotionIODevice(motionFileBuffer, i);
    }

    QString uniqId = fileNameForResource(resource->toResourcePtr());
    qint64 serverTimeZone = client::core::ServerTimeWatcher::utcOffset(resource, Qn::InvalidUtcOffset);

    QnTimePeriodList playbackMask;
    for (const auto& bookmark: d->settings.bookmarks)
    {
        if (bookmark.cameraId == resource->toResourcePtr()->getId())
            playbackMask.includeTimePeriod({bookmark.startTimeMs, bookmark.durationMs});
    }

    m_currentCamera->exportMediaPeriodToFile(d->settings.period,
        uniqId,
        lit("mkv"),
        d->storage,
        serverTimeZone,
        d->settings.transcodingSettings,
        playbackMask);

    return true;
}

void ExportLayoutTool::at_camera_exportFinished(const std::optional<nx::recording::Error>& status,
    const QString& /*filename*/)
{
    if (d->status == ExportProcessStatus::cancelled)
        return;

    d->lastError = convertError(status);
    if (d->lastError == ExportProcessError::dataNotFound)
    {
        d->lastError = ExportProcessError::noError;
        exportNextCamera();
        return;
    }

    d->exportedAnyData = true;

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

            QString uniqId = fileNameForResource(camera->resource()->toResourcePtr());
            QString motionFileName = lit("motion%1_%2.bin").arg(i).arg(uniqId);
            /* Other buffers must be closed even in case of error. */
            writeData(motionFileName, motionFileBuffer->buffer());
        }
    }

    if (error)
    {
        d->lastError = ExportProcessError::unsupportedMedia;
        finishExport(false);
    }
    else
    {
        exportNextCamera();
    }
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

} // namespace nx::vms::client::desktop
