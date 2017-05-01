#include "remote_archive_synchronizer.h"

#include <recorder/storage_manager.h>
#include <recording/stream_recorder.h>
#include <business/business_event_connector.h>
#include <transcoding/transcoding_utils.h>
#include <utils/common/util.h>
#include <motion/motion_helper.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/security_cam_resource.h>
#include <core/resource/dummy_resource.h>

#include <plugins/resource/avi/avi_archive_delegate.h>
#include <plugins/utils/avi_motion_archive_delegate.h>
#include <plugins/storage/memory/ext_iodevice_storage.h>
#include <plugins/utils/avi_motion_archive_delegate.h>

#include <nx/utils/log/log.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/random.h>
#include <nx/utils/std/cpp14.h>

namespace nx {
namespace mediaserver_core {
namespace recorder {

namespace {

static const QSize kMaxLowStreamResolution(1024, 768);
static const QString kArchiveContainer("matroska");
static const QString kArchiveContainerExtension(".mkv");

} // namespace

using namespace nx::core::resource;

RemoteArchiveSynchronizer::RemoteArchiveSynchronizer():
    m_terminated(false)
{
    connect(
        qnResPool,
        &QnResourcePool::resourceAdded,
        this,
        &RemoteArchiveSynchronizer::at_newResourceAdded);
}

RemoteArchiveSynchronizer::~RemoteArchiveSynchronizer()
{
    {
        QnMutexLocker lock(&m_mutex);
        m_terminated = true;
    }

    cancelAllTasks();
    waitForAllTasks();
}

void RemoteArchiveSynchronizer::at_newResourceAdded(const QnResourcePtr& resource)
{
    QnMutexLocker lock(&m_mutex);
    if (m_terminated)
        return;

    connect(
        resource.data(),
        &QnResource::initializedChanged,
        this,
        &RemoteArchiveSynchronizer::at_resourceInitializationChanged);
}

void RemoteArchiveSynchronizer::at_resourceInitializationChanged(const QnResourcePtr& resource)
{
    if (m_terminated)
        return;

    auto securityCameraResource = resource.dynamicCast<QnSecurityCamResource>();
    if (!securityCameraResource)
        return;

    auto archiveCanBeSynchronized = securityCameraResource->isInitialized() 
        && securityCameraResource->hasCameraCapabilities(Qn::RemoteArchiveCapability); 

    if (!archiveCanBeSynchronized)
    {
        cancelTaskForResource(securityCameraResource->getId());
        return;
    }

    auto id = securityCameraResource->getId();
    
    auto task = std::make_shared<SynchronizationTask>();
    task->setResource(securityCameraResource);
    task->setDoneHandler([this, id](){removeTaskFromAwaited(id);});

    {
        if (m_terminated)
            return;

        QnMutexLocker lock(&m_mutex);
        SynchronizationTaskContext context;
        context.task = task;
        context.result = QnConcurrent::run([task](){task->execute();});
        m_syncTasks[id] = context; 
    }
}

void RemoteArchiveSynchronizer::removeTaskFromAwaited(const QnUuid& id)
{
    QnMutexLocker lock(&m_mutex);

    auto itr = m_syncTasks.find(id);
    if (itr == m_syncTasks.end());
        return;

    m_syncTasks.erase(id);
}

void RemoteArchiveSynchronizer::cancelTaskForResource(const QnUuid& id)
{
    QnMutexLocker lock(&m_mutex);
    auto itr = m_syncTasks.find(id);
    if (itr == m_syncTasks.end())
        return;

    auto context = itr->second;
    context.task->cancel();
}

void RemoteArchiveSynchronizer::cancelAllTasks()
{
    QnMutexLocker lock(&m_mutex);
    for (auto& entry: m_syncTasks)
    {
        auto context = entry.second;
        context.task->cancel();
    }
}

void RemoteArchiveSynchronizer::waitForAllTasks()
{
    decltype(m_syncTasks) copy;
    {
        QnMutexLocker lock(&m_mutex);
        copy = m_syncTasks;
    }

    for (auto& item: copy)
    {
        auto context = item.second;
        context.result.waitForFinished();
    }
}

//------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------

void SynchronizationTask::setResource(const QnSecurityCamResourcePtr& resource)
{
    m_resource = resource;
}

void SynchronizationTask::setDoneHandler(std::function<void()> handler)
{
    m_doneHandler = handler;
}

void SynchronizationTask::cancel()
{
    m_canceled = true;
}

bool SynchronizationTask::execute()
{
    return synchronizeArchive(m_resource);
}

bool SynchronizationTask::synchronizeArchive(const QnSecurityCamResourcePtr& resource)
{
    qDebug() << "Starting archive synchronization.";
    NX_LOGX(lit("Starting archive synchronization."), cl_logDEBUG1);

    auto res = resource;
    auto manager = res->remoteArchiveManager();
    if (!manager)
        return false;

    // 1. Get list of records from camera.
    std::vector<RemoteArchiveEntry> sdCardArchiveEntries;
    manager->listAvailableArchiveEntries(&sdCardArchiveEntries);

    if (m_canceled)
        return false;

    // 2. Filter list (keep only needed records)
    auto filtered = filterEntries(resource, sdCardArchiveEntries);
    if (filtered.empty())
        return true;

    qnBusinessRuleConnector->at_remoteArchiveSyncStarted(resource);

    // 3. In cycle move all records to archive.
    for (const auto& entry : filtered)
        moveEntryToArchive(resource, entry);

    qDebug() << "Archive synchronization is done.";
    NX_LOGX(lit("Archive synchronization is done."), cl_logDEBUG1);

    qnBusinessRuleConnector->at_remoteArchiveSyncFinished(resource);
    return true;
}

std::vector<RemoteArchiveEntry> SynchronizationTask::filterEntries(
    const QnSecurityCamResourcePtr& resource,
    const std::vector<RemoteArchiveEntry>& allEntries)
{
    auto uniqueId = resource->getUniqueId();
    auto catalog = qnNormalStorageMan->getFileCatalog(
        uniqueId,
        DeviceFileCatalog::prefixByCatalog(QnServer::ChunksCatalog::HiQualityCatalog));

    std::vector<RemoteArchiveEntry> filtered;

    for (const auto& entry : allEntries)
    {
      
        if (m_canceled)
            return filtered;

        if (!catalog->containTime(entry.startTimeMs + entry.durationMs / 2))
        {
            qDebug() << "DOES NOT CONTAIN TIME" << entry.startTimeMs + entry.durationMs / 2 << entry.durationMs;
            filtered.push_back(entry);
        }
    }

    qDebug() << "Filtered entries count" << filtered.size() << ". All entries count" << allEntries.size();
    NX_LOGX(
        lm("Filtered entries count %1. All entries count %2.")
            .arg(filtered.size())
            .arg(allEntries.size()),
        cl_logDEBUG1);

    return filtered;
}

bool SynchronizationTask::moveEntryToArchive(
    const QnSecurityCamResourcePtr& resource,
    const RemoteArchiveEntry& entry)
{
    if (m_canceled)
        return false;

    BufferType buffer;
    auto manager = resource->remoteArchiveManager();
    if (!manager)
        return false;

    bool result = manager->fetchArchiveEntry(entry.id, &buffer);

    qDebug() << "Archive entry with id" << entry.id << "has been fetched, result" << result;
    NX_LOGX(
        lm("Archive entry with id %1 has been fetched, result %2")
            .arg(entry.id)
            .arg(result),
        cl_logDEBUG1);

    if (!result)
        return false;

    qDebug() << "Archive entry with id " << entry.id << " has size" << buffer.size();
    NX_LOGX(
        lm("Archive entry with id %1 has size %2")
            .arg(entry.id)
            .arg(buffer.size()),
        cl_logDEBUG1);

    return writeEntry(resource, entry, &buffer);
}

bool SynchronizationTask::writeEntry(
    const QnSecurityCamResourcePtr& resource,
    const RemoteArchiveEntry& entry,
    BufferType* buffer)
{
    QBuffer ioDevice;
    ioDevice.setBuffer(buffer);
    ioDevice.open(QIODevice::ReadOnly);

    // Obtain real media duration
    auto helper = nx::transcoding::Helper(&ioDevice);
    if (!helper.open())
    {
        qDebug() << "Can not open buffer";
        NX_LOGX(lit("Can not open buffer"), cl_logDEBUG1);
        return false;
    }

    const uint64_t kEpsilonMs = 100;
    int64_t realDurationMs = std::round(helper.durationMs());

    auto diff = entry.durationMs - realDurationMs;
    if (diff < 0)
        diff = -diff;

    if (realDurationMs == AV_NOPTS_VALUE || diff <= kEpsilonMs)
        realDurationMs = entry.durationMs;

    // Determine chunks catalog by resolution
    auto resolution = helper.resolution();
    auto catalogType = chunksCatalogByResolution(resolution);
    auto catalogPrefix = DeviceFileCatalog::prefixByCatalog(catalogType);
    auto catalog = qnNormalStorageMan->getFileCatalog(
        resource->getUniqueId(),
        catalogPrefix);

    // Ignore media if it is already in catalog
    if (catalog->containTime(entry.startTimeMs + realDurationMs / 2))
        return true;

    auto normalStorage = qnNormalStorageMan->getOptimalStorageRoot();
    if (!normalStorage)
        return false;

    auto fileName = qnNormalStorageMan->getFileName(
        entry.startTimeMs,
        currentTimeZone() / 60,
        resource,
        catalogPrefix,
        normalStorage);

    auto fileNameWithExtension = fileName + kArchiveContainerExtension;
    QFileInfo info(fileNameWithExtension);
    QDir dir = info.dir();
    if (!dir.exists() && !dir.mkpath("."))
        return false;

    qnNormalStorageMan->fileStarted(
        entry.startTimeMs,
        currentTimeZone() / 60,
        fileNameWithExtension,
        nullptr);

    auto audioLayout = helper.audioLayout();
    bool needMotion = isMotionDetectionNeeded(resource, catalogType);

    helper.close();

    bool succesfulWrite = convertAndWriteBuffer(
        resource,
        buffer,
        fileNameWithExtension,
        entry.startTimeMs,
        audioLayout,
        needMotion);

    qDebug() << "File " << fileNameWithExtension << "has been written";

    if (!succesfulWrite)
        return false; //< Should we call fileFinished?

    return qnNormalStorageMan->fileFinished(
        realDurationMs,
        fileNameWithExtension,
        nullptr,
        buffer->size());
}

bool SynchronizationTask::convertAndWriteBuffer(
    const QnSecurityCamResourcePtr& resource,
    BufferType* buffer,
    const QString& fileName,
    int64_t startTimeMs,
    const QnResourceAudioLayoutPtr& audioLayout,
    bool needMotion)
{
    qDebug() << "Converting and writing file" << fileName;
    NX_LOGX(lm("Converting and writing file %1").arg(fileName), cl_logDEBUG1);

    QBuffer* ioDevice = new QBuffer(); //< Will be freed by the owning storage.
    ioDevice->setBuffer(buffer);
    ioDevice->open(QIODevice::ReadOnly);

    const QString temporaryFilePath = QString::number(nx::utils::random::number());
    QnResourcePtr storageResource(new DummyResource());
    storageResource->setUrl(temporaryFilePath); //< Check it if something doesn't work.

    QnExtIODeviceStorageResourcePtr storage(new QnExtIODeviceStorageResource());
    storage->registerResourceData(temporaryFilePath, ioDevice);

    using namespace  nx::mediaserver_core::plugins;
    std::shared_ptr<QnAviArchiveDelegate> reader(needMotion
        ? new QnAviArchiveDelegate()
        : new AviMotionArchiveDelegate());

    reader->setStorage(storage);
    if (!reader->open(storageResource))
        return false;

    reader->setAudioChannel(0);
    reader->setMotionRegion(resource->getMotionRegion(0));
    reader->setStartTimeUs(startTimeMs * 1000);

    auto saveMotionHandler =
        [&resource](const QnConstMetaDataV1Ptr& motion)
        {
            if (motion)
            {
                auto helper = QnMotionHelper::instance();
                QnMotionArchive* archive = helper->getArchive(
                    resource,
                    motion->channelNumber);

                if (archive)
                    archive->saveToArchive(motion);
            }
            return true;
        };

    QnStreamRecorder recorder(resource);
    recorder.clearUnprocessedData();
    recorder.addRecordingContext(fileName, qnNormalStorageMan->getOptimalStorageRoot());
    recorder.setContainer(kArchiveContainer);
    recorder.forceAudioLayout(audioLayout);
    recorder.disableRegisterFile(true);
    recorder.setSaveMotionHandler(saveMotionHandler);

    auto connection = QObject::connect(
        resource.data(),
        &QnSecurityCamResource::motionRegionChanged,
        [&reader, &resource](const QnResourcePtr& res)
        {
            reader->setMotionRegion(resource->getMotionRegion(0));
        });

    while (auto mediaData = reader->getNextData())
    {
        if (m_canceled)
            break;

        if (mediaData->dataType == QnAbstractMediaData::EMPTY_DATA)
            break;

        recorder.processData(mediaData);
    }

    QObject::disconnect(connection); //< TODO: #dmishin possible crash here

    return true;
}

QnServer::ChunksCatalog SynchronizationTask::chunksCatalogByResolution(const QSize& resolution) const
{
    const auto maxLowStreamArea = kMaxLowStreamResolution.width() * kMaxLowStreamResolution.height();
    const auto area = resolution.width() * resolution.height();

    return area <= maxLowStreamArea
        ? QnServer::ChunksCatalog::LowQualityCatalog
        : QnServer::ChunksCatalog::HiQualityCatalog;
}

bool SynchronizationTask::isMotionDetectionNeeded(
    const QnSecurityCamResourcePtr& resource,
    QnServer::ChunksCatalog catalog) const
{
    if (catalog == QnServer::ChunksCatalog::LowQualityCatalog)
    {
        return true;
    }
    else if (catalog == QnServer::ChunksCatalog::HiQualityCatalog)
    {
        auto streamForMotionDetection = resource->getProperty(QnMediaResource::motionStreamKey()).toLower();
        return streamForMotionDetection == QnMediaResource::primaryStreamValue();
    }

    return false;
}

} // namespace reorder
} // namespace mediaserver_core
} // namespace nx
