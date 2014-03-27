#include "workbench_layout_export_tool.h"

#include <QtCore/QBuffer>


#include <client/client_settings.h>

#include <camera/caching_time_period_loader.h>
#include <camera/client_video_camera.h>

#include <core/resource/resource.h>
#include <core/resource/media_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/resource_directory_browser.h>
#include <core/resource_management/resource_pool.h>

#include <plugins/resources/archive/avi_files/avi_resource.h>
#include <plugins/storage/file_storage/layout_storage_resource.h>

#include <recording/stream_recorder.h>

#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_navigator.h>
#include <ui/workbench/workbench_layout_snapshot_manager.h>
#include <ui/workbench/watchers/workbench_server_time_watcher.h>

#include <utils/app_server_image_cache.h>
#include <utils/local_file_cache.h>

#ifdef Q_OS_WIN
#include <launcher/nov_launcher_win.h>
#endif
#include "nx_ec/data/ec2_layout_data.h"

QnLayoutExportTool::QnLayoutExportTool(const QnLayoutResourcePtr &layout,
                                       const QnTimePeriod &period,
                                       const QString &filename,
                                       Qn::LayoutExportMode mode,
                                       bool readOnly,
                                       QObject *parent) :
    QObject(parent),
    QnWorkbenchContextAware(parent),
    m_period(period),
    m_targetFilename(QnLayoutFileStorageResource::removeProtocolPrefix(filename)),
    m_realFilename(m_targetFilename),
    m_mode(mode),
    m_readOnly(readOnly),
    m_offset(-1),
    m_stopped(false),
    m_currentCamera(0)
{
    m_layout.reset(new QnLayoutResource());
    m_layout->setId(layout->getId());
    m_layout->setGuid(layout->getGuid()); //before update() uuid's must be the same
    m_layout->update(layout);

    // If exporting layout, create new guid. If layout just renamed, keep guid
    if (mode == Qn::LayoutExport)
        m_layout->setGuid(QUuid::createUuid());
}

bool QnLayoutExportTool::start() {
    if (m_realFilename == QnLayoutFileStorageResource::removeProtocolPrefix(m_layout->getUrl())) {
        // can not override opened layout. save to tmp file, then rename
        m_realFilename += QLatin1String(".tmp");
    }

#ifdef Q_OS_WIN
    if (m_targetFilename.endsWith(QLatin1String(".exe")))
    {
        if (QnNovLauncher::createLaunchingFile(m_realFilename) != 0)
        {
            m_errorMessage = tr("File '%1' is used by another process. Please try another name.").arg(QFileInfo(m_realFilename).completeBaseName());
            emit finished(false, m_targetFilename);   //file is not created, finishExport() is not required
            return false;
        }
    }
    else
#endif
    {
        QFile::remove(m_realFilename);
    }

    QString fullName = QnLayoutFileStorageResource::layoutPrefix() + m_realFilename;

    m_storage = QnStorageResourcePtr(new QnLayoutFileStorageResource());
    m_storage->setUrl(fullName);

    QIODevice* itemNamesIO = m_storage->open(QLatin1String("item_names.txt"), QIODevice::WriteOnly);
    QTextStream itemNames(itemNamesIO);

    QList<qint64> itemTimeZones;

    QSet<QString> uniqIdList;
    QnLayoutItemDataMap items;

    foreach (QnLayoutItemData item, m_layout->getItems()) {
        QnResourcePtr resource = qnResPool->getResourceById(item.resource.id);
        if (!resource)
            resource = qnResPool->getResourceByUniqId(item.resource.path);
        if (!resource)
            continue;

        QnLayoutItemData localItem = item;
        itemNames << resource->getName() << QLatin1String("\n");
        QnMediaResourcePtr mediaRes = qSharedPointerDynamicCast<QnMediaResource>(resource);
        if (mediaRes) {
            QString uniqueId = mediaRes->toResource()->getUniqueId();
            localItem.resource.id = 0;
            localItem.resource.path = uniqueId;
            if (!uniqIdList.contains(uniqueId)) {
                m_resources << mediaRes;
                uniqIdList << uniqueId;
            }
            itemTimeZones << context()->instance<QnWorkbenchServerTimeWatcher>()->utcOffset(mediaRes, Qn::InvalidUtcOffset);
        }
        else {
            itemTimeZones << Qn::InvalidUtcOffset;
        }
        items.insert(localItem.uuid, localItem);
    }
    m_layout->setItems(items);

    itemNames.flush();
    delete itemNamesIO;

    QIODevice* itemTimezonesIO = m_storage->open(QLatin1String("item_timezones.txt"), QIODevice::WriteOnly);
    QTextStream itemTimeZonesStream(itemTimezonesIO);
    foreach(qint64 timeZone, itemTimeZones)
        itemTimeZonesStream << timeZone << QLatin1String("\n");
    itemTimeZonesStream.flush();
    delete itemTimezonesIO;

    QByteArray layoutData;
    ec2::ApiLayoutData layoutObject;
    layoutObject.fromResource(m_layout);
    OutputBinaryStream<QByteArray> stream(&layoutData);
    serialize(layoutObject, &stream);


    QIODevice* device = m_storage->open(QLatin1String("layout.pb"), QIODevice::WriteOnly);
    device->write(layoutData);
    delete device;

    device = m_storage->open(QLatin1String("range.bin"), QIODevice::WriteOnly);
    device->write(m_period.serialize());
    delete device;

    device = m_storage->open(QLatin1String("misc.bin"), QIODevice::WriteOnly);
    quint32 flags = m_readOnly ? QnLayoutFileStorageResource::ReadOnly : 0;

    foreach (const QnMediaResourcePtr resource, m_resources) {
        if (resource->toResource()->hasFlags(QnResource::utc)) {
            flags |= QnLayoutFileStorageResource::ContainsCameras;
            break;
        }
    }
    device->write((const char*) &flags, sizeof(flags));
    delete device;

    device = m_storage->open(QLatin1String("uuid.bin"), QIODevice::WriteOnly);
    device->write(m_layout->getGuid().toByteArray());
    delete device;

    foreach (const QnMediaResourcePtr resource, m_resources) {
        QString uniqId = resource->toResource()->getUniqueId();
        uniqId = uniqId.mid(uniqId.lastIndexOf(L'?') + 1);
        QnCachingTimePeriodLoader* loader = navigator()->loader(resource->toResourcePtr());
        if (loader) {
            QIODevice* device = m_storage->open(QString(QLatin1String("chunk_%1.bin")).arg(QFileInfo(uniqId).completeBaseName()), QIODevice::WriteOnly);
            QnTimePeriodList periods = loader->periods(Qn::RecordingContent).intersected(m_period);
            QByteArray data;
            periods.encode(data);
            device->write(data);
            delete device;
        }
    }

    if (!m_layout->backgroundImageFilename().isEmpty()) {
        QnAppServerImageCache cache(this);
        QImage backround(cache.getFullPath(m_layout->backgroundImageFilename()));
        if (!backround.isNull()) {
            device = m_storage->open(m_layout->backgroundImageFilename(), QIODevice::WriteOnly);
            backround.save(device, "png");
            delete device;

            QnLocalFileCache cache;
            cache.storeImage(m_layout->backgroundImageFilename(), backround);
        }
    }

    emit rangeChanged(0, m_resources.size() * 100);
    emit valueChanged(0);
    return exportNextCamera();
}

void QnLayoutExportTool::stop() {
    m_stopped = true;
    m_resources.clear();
    m_errorMessage = QString(); //supress error by manual cancelling

    if (m_currentCamera) {
        connect(m_currentCamera, SIGNAL(exportStopped()), this, SLOT(at_camera_exportStopped()));
        m_currentCamera->stopExport();
    } else {
        finishExport(false);
    }
}

Qn::LayoutExportMode QnLayoutExportTool::mode() const {
    return m_mode;
}

QString QnLayoutExportTool::errorMessage() const {
    return m_errorMessage;
}

bool QnLayoutExportTool::exportNextCamera() {
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

void QnLayoutExportTool::finishExport(bool success) {

    if (success) {
        if (m_realFilename != m_targetFilename)
        {
            m_storage->renameFile(m_storage->getUrl(), QnLayoutFileStorageResource::layoutPrefix() + m_targetFilename);
            if (m_mode == Qn::LayoutLocalSave) {
                QnLayoutResourcePtr layout = resourcePool()->getResourceByUniqId(m_layout->getUniqueId()).dynamicCast<QnLayoutResource>();
                if (layout) {
                    layout->update(m_layout);
                    snapshotManager()->store(layout);
                }
            } else {
                snapshotManager()->store(m_layout);
            }
        }
        else if (m_mode == Qn::LayoutLocalSaveAs)
        {
            QString oldUrl = m_layout->getUrl();
            QString newUrl = m_storage->getUrl();

            foreach (const QnLayoutItemData &item, m_layout->getItems()) {
                QnAviResourcePtr aviRes = qnResPool->getResourceByUniqId(item.resource.path).dynamicCast<QnAviResource>();
                if (aviRes)
                    qnResPool->updateUniqId(aviRes, QnLayoutFileStorageResource::updateNovParent(newUrl, item.resource.path));
            }
            m_layout->setUrl(newUrl);
            m_layout->setName(QFileInfo(newUrl).fileName());

            QnLayoutFileStorageResourcePtr novStorage = m_storage.dynamicCast<QnLayoutFileStorageResource>();
            if (novStorage)
                novStorage->switchToFile(oldUrl, newUrl, false);
            snapshotManager()->store(m_layout);
        }
        else {
            QnLayoutResourcePtr layout =  QnResourceDirectoryBrowser::layoutFromFile(m_storage->getUrl());
            if (!resourcePool()->getResourceByGuid(layout->getUniqueId())) {
                layout->setStatus(QnResource::Online);
                resourcePool()->addResource(layout);
            }
        }
    } else {
        QFile::remove(m_realFilename);
    }
    emit finished(success, m_targetFilename);
}

bool QnLayoutExportTool::exportMediaResource(const QnMediaResourcePtr& resource) {
    m_currentCamera = new QnClientVideoCamera(resource);
    connect(m_currentCamera,    SIGNAL(exportProgress(int)),            this,   SLOT(at_camera_progressChanged(int)));
    connect(m_currentCamera,    &QnClientVideoCamera::exportFinished,   this,   &QnLayoutExportTool::at_camera_exportFinished);

    int numberOfChannels = resource->getVideoLayout()->channelCount();
    for (int i = 0; i < numberOfChannels; ++i) {
        QSharedPointer<QBuffer> motionFileBuffer(new QBuffer());
        motionFileBuffer->open(QIODevice::ReadWrite);
        m_currentCamera->setMotionIODevice(motionFileBuffer, i);
    }

    QString uniqId = resource->toResource()->getUniqueId();
    uniqId = uniqId.mid(uniqId.indexOf(L'?')+1); // simplify name if export from existing layout
    QnStreamRecorder::Role role = QnStreamRecorder::Role_FileExport;
    if (resource->toResource()->hasFlags(QnResource::utc))
        role = QnStreamRecorder::Role_FileExportWithEmptyContext;
    QnLayoutItemData itemData = m_layout->getItem(uniqId);

    int timeOffset = 0;
    if(qnSettings->timeMode() == Qn::ServerTimeMode) {
        // time difference between client and server
        timeOffset = context()->instance<QnWorkbenchServerTimeWatcher>()->localOffset(resource, 0);
    }
    qint64 serverTimeZone = context()->instance<QnWorkbenchServerTimeWatcher>()->utcOffset(resource, Qn::InvalidUtcOffset);

    m_currentCamera->exportMediaPeriodToFile(m_period.startTimeMs * 1000ll,
                                    (m_period.startTimeMs + m_period.durationMs) * 1000ll,
                                    uniqId,
                                    QLatin1String("mkv"),
                                    m_storage,
                                    role,
                                    Qn::NoCorner,
                                    timeOffset, serverTimeZone,
                                    itemData.zoomRect,
                                    itemData.contrastParams,
                                    itemData.dewarpingParams);

    emit stageChanged(tr("Exporting %1 to \"%2\"...").arg(resource->toResource()->getUrl()).arg(m_targetFilename));
    return true;
}

void QnLayoutExportTool::at_camera_exportFinished(int status, const QString &filename) {
    Q_UNUSED(filename)
    if (status != QnClientVideoCamera::NoError) {
        m_errorMessage = QnClientVideoCamera::errorString(status);
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
            QString motionFileName = QString(QLatin1String("motion%1_%2.bin")).arg(i).arg(uniqId);
            QIODevice* device = m_storage->open(motionFileName , QIODevice::WriteOnly);

            int retryCount = 0;
            while (!device && retryCount < 3)
            {
                QEventLoop loop;
                QTimer::singleShot(500, &loop, SLOT(quit()));
                loop.exec();
                device = m_storage->open(motionFileName , QIODevice::WriteOnly);
                retryCount++;
            }

            if (!device) {
                error = true;
                continue; //need to close other buffers
            }

            device->write(motionFileBuffer->buffer());
            device->close();
        }
    }

    camera->deleteLater();

    if (error) {
        m_errorMessage = tr("Could not export camera %1").arg(camera->resource()->toResource()->getName());
        finishExport(false);
    }
    else {
        exportNextCamera();
    }
}

void QnLayoutExportTool::at_camera_exportStopped() {
    if (m_currentCamera)
        m_currentCamera->deleteLater();

    finishExport(false);
}

void QnLayoutExportTool::at_camera_progressChanged(int progress) {
    emit valueChanged(m_offset * 100 + progress);
}
