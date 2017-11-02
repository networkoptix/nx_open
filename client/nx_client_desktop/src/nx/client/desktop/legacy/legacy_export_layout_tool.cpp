#include "legacy_export_layout_tool.h"

#include <QtCore/QBuffer>
#include <QtCore/QEventLoop>

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
#include <core/resource/resource_directory_browser.h>
#include <core/resource_management/resource_pool.h>

#include <nx_ec/data/api_layout_data.h>
#include <nx_ec/data/api_conversion_functions.h>

#include <core/storage/file_storage/layout_storage_resource.h>
#include <core/resource/avi/avi_resource.h>

#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_layout_snapshot_manager.h>
#include <ui/workbench/watchers/workbench_server_time_watcher.h>

#include <nx/fusion/model_functions.h>
#include <nx/client/desktop/utils/server_image_cache.h>
#include <nx/client/desktop/utils/local_file_cache.h>
#include <client_core/client_core_module.h>

#include <nx/utils/app_info.h>

#ifdef Q_OS_WIN
#   include <launcher/nov_launcher_win.h>
#endif

namespace {
    const int retryTimes = 3;
    const int retryDelayMs = 500;
}

namespace nx {
namespace client {
namespace desktop {
namespace legacy {

ExportLayoutTool::ItemInfo::ItemInfo()
    : name()
    , timezone(0)
{}

ExportLayoutTool::ItemInfo::ItemInfo( const QString name, qint64 timezone )
    : name(name)
    , timezone(timezone)
{}


ExportLayoutTool::ExportLayoutTool(ExportLayoutSettings settings, QObject* parent) :
    QObject(parent),
    QnWorkbenchContextAware(parent),
    m_settings(settings),
    m_realFilename(m_settings.filename.completeFileName())
{
    m_layout.reset(new QnLayoutResource());
    m_layout->setId(m_settings.layout->getId()); //before update() uuid's must be the same
    m_layout->update(m_settings.layout);

    // If exporting layout, create new guid. If layout just renamed, keep guid
    if (m_settings.mode != ExportLayoutSettings::Mode::LocalSave)
        m_layout->setId(QnUuid::createUuid());
}


bool ExportLayoutTool::prepareStorage()
{
    const auto isExeFile = nx::utils::AppInfo::isWindows()
        && FileExtensionUtils::isExecutable(m_settings.filename.extension);

    if (isExeFile || m_realFilename == m_layout->getUrl())
    {
        // can not override opened layout. save to tmp file, then rename
        m_realFilename += lit(".tmp");
    }

#ifdef Q_OS_WIN
    if (isExeFile)
    {
        // TODO: #GDM handle other errors
        if (QnNovLauncher::createLaunchingFile(m_realFilename) != QnNovLauncher::ErrorCode::Ok)
        {
            m_errorMessage = tr("File \"%1\" is used by another process. Please try another name.").arg(QFileInfo(m_realFilename).completeBaseName());
            emit finished(false, m_settings.filename.completeFileName());   //file is not created, finishExport() is not required
            return false;
        }
    }
    else
#endif
    {
        QFile::remove(m_realFilename);
    }

    m_storage = QnStorageResourcePtr(new QnLayoutFileStorageResource(qnClientCoreModule->commonModule()));
    m_storage->setUrl(m_realFilename);
    return true;
}

ExportLayoutTool::ItemInfoList ExportLayoutTool::prepareLayout()
{
    ItemInfoList result;

    QSet<QString> uniqIdList;
    QnLayoutItemDataMap items;

    for (const QnLayoutItemData &item: m_layout->getItems())
    {
        QnResourcePtr resource = resourcePool()->getResourceByDescriptor(item.resource);
        if (!resource)
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
                m_resources << mediaRes;
                uniqIdList << uniqueId;
            }
            info.timezone = context()->instance<QnWorkbenchServerTimeWatcher>()->utcOffset(mediaRes, Qn::InvalidUtcOffset);
        }

        items.insert(localItem.uuid, localItem);
        result.append(info);
    }
    m_layout->setItems(items);
    return result;
}


bool ExportLayoutTool::exportMetadata(const ExportLayoutTool::ItemInfoList &items) {

    /* Names of exported resources. */
    {
        QByteArray names;
        QTextStream itemNames(&names, QIODevice::WriteOnly);
        for (const ItemInfo &info: items)
            itemNames << info.name << lit("\n");
        itemNames.flush();

        if (!writeData(lit("item_names.txt"), names))
            return false;
    }

    /* Timezones of exported resources. */
    {
        QByteArray timezones;
        QTextStream itemTimeZonesStream(&timezones, QIODevice::WriteOnly);
        for (const ItemInfo &info: items)
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
        if (!writeData(lit("range.bin"), m_settings.period.serialize()))
            return false;
    }

    /* Additional flags. */
    {
        quint32 flags = m_settings.readOnly ? QnLayoutFileStorageResource::ReadOnly : 0;
        for (const QnMediaResourcePtr resource: m_resources) {
            if (resource->toResource()->hasFlags(Qn::utc)) {
                flags |= QnLayoutFileStorageResource::ContainsCameras;
                break;
            }
        }

        if (!tryAsync([this, flags] {
            QScopedPointer<QIODevice> miscFile(m_storage->open(lit("misc.bin"), QIODevice::WriteOnly));
            if (!miscFile)
                return false;
            miscFile->write((const char*) &flags, sizeof(flags));
            return true;
        } ))
            return false;
    }

    /* Layout id. */
    {
        if (!writeData(lit("uuid.bin"), m_layout->getId().toByteArray()))
            return false;
    }

    /* Chunks. */
    for (const QnMediaResourcePtr &resource: m_resources) {
        QString uniqId = resource->toResource()->getUniqueId();
        uniqId = uniqId.mid(uniqId.lastIndexOf(L'?') + 1);
        auto loader = qnClientModule->cameraDataManager()->loader(resource);
        if (!loader)
            continue;
        QnTimePeriodList periods = loader->periods(Qn::RecordingContent).intersected(m_settings.period);
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
        if (!background.isNull()) {

            if (!tryAsync([this, background] {
                QScopedPointer<QIODevice> imageFile(m_storage->open(m_layout->backgroundImageFilename(), QIODevice::WriteOnly));
                if (!imageFile)
                    return false;
                background.save(imageFile.data(), "png");
                return true;
            } ))
                return false;

            LocalFileCache localCache;
            localCache.storeImageData(m_layout->backgroundImageFilename(), background);
        }
    }

    return true;
}

bool ExportLayoutTool::start() {
    if (!prepareStorage())
        return false;

    ItemInfoList items = prepareLayout();

    if (!exportMetadata(items)) {
        m_errorMessage = tr("Could not create output file %1...").arg(m_settings.filename.completeFileName());
        emit finished(false, m_settings.filename.completeFileName());   //file is not created, finishExport() is not required
        return false;
    }

    emit rangeChanged(0, m_resources.size() * 100);
    emit valueChanged(0);
    return exportNextCamera();
}

void ExportLayoutTool::stop() {
    m_stopped = true;
    m_resources.clear();
    m_errorMessage = QString(); //suppress error by manual canceling

    if (m_currentCamera) {
        connect(m_currentCamera, SIGNAL(exportStopped()), this, SLOT(at_camera_exportStopped()));
        m_currentCamera->stopExport();
    } else {
        finishExport(false);
    }
}

ExportLayoutSettings::Mode ExportLayoutTool::mode() const {
    return m_settings.mode;
}

QString ExportLayoutTool::errorMessage() const {
    return m_errorMessage;
}

bool ExportLayoutTool::exportNextCamera() {
    if (m_stopped)
        return false;

    if (m_resources.isEmpty()) {
        finishExport(true);
        return false;
    } else {
        m_offset++;
        return exportMediaResource(m_resources.dequeue());
    }
}

void ExportLayoutTool::finishExport(bool success)
{
    if (!success)
    {
        QFile::remove(m_realFilename);
        emit finished(false, m_settings.filename.completeFileName());
        return;
    }

    if (m_realFilename != m_settings.filename.completeFileName())
        m_storage->renameFile(m_storage->getUrl(), m_settings.filename.completeFileName());

    auto existing = resourcePool()->getResourceByUrl(m_settings.filename.completeFileName())
        .dynamicCast<QnLayoutResource>();

    switch (m_settings.mode)
    {
        case ExportLayoutSettings::Mode::LocalSave:
        {
            /* Update existing layout. */
            NX_ASSERT(existing);
            if (existing)
            {
                existing->update(m_layout);
                snapshotManager()->store(existing);
            }
            break;
        }
        case ExportLayoutSettings::Mode::LocalSaveAs:
        case ExportLayoutSettings::Mode::Export:
        {
            /* Existing is present if we did 'Save As..' with another existing layout name. */
            if (existing)
                resourcePool()->removeResources(existing->layoutResources().toList() << existing);

            auto layout = QnResourceDirectoryBrowser::layoutFromFile(m_storage->getUrl(), resourcePool());
            if (!layout)
            {
                /* Something went wrong */
                m_errorMessage = tr("Unknown error has occurred.");
                QFile::remove(m_realFilename);
                emit finished(false, m_settings.filename.completeFileName());
                return;
            }
            resourcePool()->addResource(layout);
            break;
        }
        default:
            break;
    }
    emit finished(success, m_settings.filename.completeFileName());

    /*
    else if (m_mode == Qn::LayoutLocalSaveAs)
    {
        QString oldUrl = m_layout->getUrl();
        QString newUrl = m_storage->getUrl();

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

        QnLayoutFileStorageResourcePtr novStorage = m_storage.dynamicCast<QnLayoutFileStorageResource>();
        if (novStorage)
            novStorage->switchToFile(oldUrl, newUrl, false);
        snapshotManager()->store(m_layout);
    }
    else
    */

}

bool ExportLayoutTool::exportMediaResource(const QnMediaResourcePtr& resource) {
    m_currentCamera = new QnClientVideoCamera(resource);
    connect(m_currentCamera,    SIGNAL(exportProgress(int)),            this,   SLOT(at_camera_progressChanged(int)));
    connect(m_currentCamera,    &QnClientVideoCamera::exportFinished,   this,   &ExportLayoutTool::at_camera_exportFinished);

    int numberOfChannels = resource->getVideoLayout()->channelCount();
    for (int i = 0; i < numberOfChannels; ++i) {
        QSharedPointer<QBuffer> motionFileBuffer(new QBuffer());
        motionFileBuffer->open(QIODevice::ReadWrite);
        m_currentCamera->setMotionIODevice(motionFileBuffer, i);
    }

    QString uniqId = resource->toResource()->getUniqueId();
    uniqId = uniqId.mid(uniqId.indexOf(L'?')+1); // simplify name if export from existing layout
    auto role = StreamRecorderRole::fileExport;
    if (resource->toResource()->hasFlags(Qn::utc))
        role = StreamRecorderRole::fileExportWithEmptyContext;

    qint64 serverTimeZone = context()->instance<QnWorkbenchServerTimeWatcher>()->utcOffset(resource, Qn::InvalidUtcOffset);

    m_currentCamera->exportMediaPeriodToFile(m_settings.period,
        uniqId,
        lit("mkv"),
        m_storage,
        role,
        serverTimeZone,
        0,
        nx::core::transcoding::FilterChain());

    emit stageChanged(tr("Exporting to \"%1\"...").arg(QFileInfo(m_settings.filename.completeFileName()).fileName()));
    return true;
}

void ExportLayoutTool::at_camera_exportFinished(const StreamRecorderErrorStruct& status,
    const QString& filename)
{
    Q_UNUSED(filename)
    if (status.lastError != StreamRecorderError::noError)
    {
        m_errorMessage = QnStreamRecorder::errorString(status.lastError);
        finishExport(false);
        return;
    }

    QnClientVideoCamera* camera = dynamic_cast<QnClientVideoCamera*>(sender());
    if (!camera)
        return;

    int numberOfChannels = camera->resource()->getVideoLayout()->channelCount();
    bool error = false;

    for (int i = 0; i < numberOfChannels; ++i) {
        if (QSharedPointer<QBuffer> motionFileBuffer = camera->motionIODevice(i)) {
            motionFileBuffer->close();
            if (error)
                continue;

            QString uniqId = camera->resource()->toResource()->getUniqueId();
            uniqId = QFileInfo(uniqId.mid(uniqId.indexOf(L'?')+1)).completeBaseName(); // simplify name if export from existing layout
            QString motionFileName = lit("motion%1_%2.bin").arg(i).arg(uniqId);
            /* Other buffers must be closed even in case of error. */
            writeData(motionFileName, motionFileBuffer->buffer());
        }
    }

    camera->deleteLater();

    if (error)
    {
        QnVirtualCameraResourcePtr camRes = camera->resource()->toResourcePtr().dynamicCast<QnVirtualCameraResource>();
        NX_ASSERT(camRes, Q_FUNC_INFO, "Make sure camera exists");
        //: "Could not export camera AXIS1334"
        m_errorMessage = QnDeviceDependentStrings::getNameFromSet(
            resourcePool(),
            QnCameraDeviceStringSet(
                tr("Could not export device %1."),
                tr("Could not export camera %1."),
                tr("Could not export I/O module %1.")
            ), camRes)
            .arg(QnResourceDisplayInfo(camRes).toString(Qn::RI_NameOnly));
        finishExport(false);
    }
    else
    {
        exportNextCamera();
    }
}

void ExportLayoutTool::at_camera_exportStopped() {
    if (m_currentCamera)
        m_currentCamera->deleteLater();

    finishExport(false);
}

void ExportLayoutTool::at_camera_progressChanged(int progress) {
    emit valueChanged(m_offset * 100 + progress);
}

bool ExportLayoutTool::tryAsync( std::function<bool()> handler ) {
    if (!handler)
        return false;

    QPointer<QObject> guard(this);
    int times = retryTimes;

    while (times > 0) {
        if (!guard)
            return false;

        if (handler())
            return true;

        times--;
        if (times <= 0)
            return false;

        QScopedPointer<QTimer> timer(new QTimer());
        timer->setSingleShot(true);
        timer->setInterval(retryDelayMs);

        QScopedPointer<QEventLoop> loop(new QEventLoop());
        connect(timer.data(), &QTimer::timeout, loop.data(), &QEventLoop::quit);
        timer->start();
        loop->exec();
    }

    return false;
}

bool ExportLayoutTool::writeData( const QString &fileName, const QByteArray &data ) {
    return tryAsync([this, fileName, data] {
        QScopedPointer<QIODevice> file(m_storage->open(fileName, QIODevice::WriteOnly));
        if (!file)
            return false;
        file->write(data);
        return true;
    } );
}

} // namespace legacy
} // namespace desktop
} // namespace client
} // namespace nx
