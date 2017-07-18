#include "remote_archive_synchronizer.h"

#include <recorder/storage_manager.h>
#include <recording/stream_recorder.h>
#include <nx/mediaserver/event/event_connector.h>
#include <transcoding/transcoding_utils.h>
#include <utils/common/util.h>
#include <motion/motion_helper.h>
#include <database/server_db.h>
#include <common/common_module.h>
#include <common/static_common_module.h>

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

RemoteArchiveSynchronizer::RemoteArchiveSynchronizer(QObject* parent):
    QnCommonModuleAware(parent),
    m_terminated(false)
{
    qDebug() << "Creating remote archive synchronizer.";
    NX_LOGX(lit("Starting archive synchronization."), cl_logDEBUG1);

    connect(
        commonModule()->resourcePool(),
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

    qDebug() << "!!!! Attention !!!! RemoteArchiveSynchronizer destructor";
    cancelAllTasks();
    waitForAllTasks();
}

void RemoteArchiveSynchronizer::at_newResourceAdded(const QnResourcePtr& resource)
{
    QnMutexLocker lock(&m_mutex);
    if (m_terminated)
        return;

    auto securityCameraResource = resource.dynamicCast<QnSecurityCamResource>();
    if (!securityCameraResource)
        return;

    connect(
        securityCameraResource.data(),
        &QnResource::initializedChanged,
        this,
        &RemoteArchiveSynchronizer::at_resourceInitializationChanged);

    connect(
        securityCameraResource.data(),
        &QnSecurityCamResource::scheduleDisabledChanged,
        this,
        &RemoteArchiveSynchronizer::at_resourceInitializationChanged);

    connect(
        securityCameraResource.data(),
        &QnResource::parentIdChanged,
        this,
        &RemoteArchiveSynchronizer::at_resourceParentIdChanged);
}

void RemoteArchiveSynchronizer::at_resourceInitializationChanged(const QnResourcePtr& resource)
{
    if (m_terminated || resource->hasFlags(Qn::foreigner))
        return;

    auto securityCameraResource = resource.dynamicCast<QnSecurityCamResource>();
    if (!securityCameraResource)
        return;

    auto archiveCanBeSynchronized = securityCameraResource->isInitialized() 
        && securityCameraResource->hasCameraCapabilities(Qn::RemoteArchiveCapability)
        && securityCameraResource->isLicenseUsed()
        && !securityCameraResource->isScheduleDisabled(); 

    if (!archiveCanBeSynchronized)
    {
        qDebug()
            << "Archive for resource"
            << securityCameraResource->getName()
            << "can not be synchronized. Resource is initialized:" 
            << securityCameraResource->isInitialized()
            << "Resource has remote archive capability:" 
            << securityCameraResource->hasCameraCapabilities(Qn::RemoteArchiveCapability)
            << "License used for resource:"
            << securityCameraResource->isLicenseUsed()
            << "Schedule is enabled for resource:"
            << !securityCameraResource->isScheduleDisabled();
        cancelTaskForResource(securityCameraResource->getId());
        return;
    }

    {
        QnMutexLocker lock(&m_mutex);
        makeAndRunTaskUnsafe(securityCameraResource);
    }
}

void RemoteArchiveSynchronizer::at_resourceParentIdChanged(const QnResourcePtr& resource)
{
    qDebug() << "!!!! ATTENTION !!!!! resource parent ID has been changed"
        << resource->getParentId();

    cancelTaskForResource(resource->getId());
}

void RemoteArchiveSynchronizer::makeAndRunTaskUnsafe(const QnSecurityCamResourcePtr& resource)
{
    if (m_terminated)
        return;

    auto id = resource->getId();
    auto task = std::make_shared<SynchronizationTask>(commonModule());
    task->setResource(resource);
    task->setDoneHandler([this, id]() { removeTaskFromAwaited(id); });

    SynchronizationTaskContext context;
    context.task = task;
    context.result = nx::utils::concurrent::run([task]() { task->execute(); });
    m_syncTasks[id] = context;    
}

void RemoteArchiveSynchronizer::removeTaskFromAwaited(const QnUuid& id)
{
    QnMutexLocker lock(&m_mutex);
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


SynchronizationTask::SynchronizationTask(QnCommonModule* commonModule):
    m_commonModule(commonModule),
    m_canceled(false),
    m_totalChunksToSynchronize(0),
    m_currentNumberOfSynchronizedChunks(0)
{
}

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
    {
        qDebug() << "Resource" << resource->getName() << "has no remote archive manager";
        NX_LOGX(
            lm("Resource %1 has no remote archive manager")
                .arg(resource->getName()),
            cl_logDEBUG1);

        return false;
    }

    // 1. Get list of records from camera.
    std::vector<RemoteArchiveEntry> sdCardArchiveEntries;
    manager->listAvailableArchiveEntries(&sdCardArchiveEntries);

    if (m_canceled)
    {
        qDebug() << "Remote archive synchronization task for resource" << resource->getName() << "was canceled";
        NX_LOGX(
            lm("Remote archive synchronization task for resource %1 was canceled")
                .arg(resource->getName()),
            cl_logDEBUG1);
        return false;
    }

    // 2. Filter list (keep only needed records)
    auto filtered = filterEntries(resource, sdCardArchiveEntries);
    if (filtered.empty())
    {
        qDebug() << "Archive is fully synchronized. There is no need in any further actions";
        NX_LOGX(lit("Archive is fully synchronized. There is no need in any further actions"), cl_logDEBUG1);
        return true;
    }

    auto totalEntriesToSynchronize = filtered.size();
    int currentNumberOfSynchronizedEntries = 0;
    double lastBroadcastedSyncProgress = 0; 
    const int kNumberOfSynchronizedEntriesToNotify = 5;
    double broadcastSyncProgressStep = 
        kNumberOfSynchronizedEntriesToNotify / (double)totalEntriesToSynchronize;

    if (broadcastSyncProgressStep > 0.7)
        broadcastSyncProgressStep = 1;

    qnEventRuleConnector->at_remoteArchiveSyncStarted(resource);

    // 3. In cycle copy all records to archive.
    for (const auto& entry : filtered)
    {
        qDebug() << "Copying entry to archive";
        bool result = copyEntryToArchive(resource, entry);

        if (!result)
        {
            qDebug() << lit("Can not synchronize video chunk.");
            NX_LOGX(lit("Can not synchronize video chunk."), cl_logDEBUG1);
            qnEventRuleConnector->at_remoteArchiveSyncError(
                resource,
                lit("Can not synchronize video chunk."));
        }

        ++currentNumberOfSynchronizedEntries;

        double currentSyncProgress = 
            (double) currentNumberOfSynchronizedEntries / totalEntriesToSynchronize;

        if (currentSyncProgress - lastBroadcastedSyncProgress > broadcastSyncProgressStep)
        {
            qDebug() << lit("Remote archive synchronization progress is %1").arg(currentSyncProgress);
            NX_LOGX(lm("Remote archive synchronization progress is %1").arg(currentSyncProgress), cl_logDEBUG1);
            qnEventRuleConnector->at_remoteArchiveSyncProgress(resource, currentSyncProgress);
            qDebug() << "Progress action has been broadcasted";
            lastBroadcastedSyncProgress = currentSyncProgress;
        }
    }

    qDebug() << "Archive synchronization is done.";
    NX_LOGX(lit("Archive synchronization is done."), cl_logDEBUG1);

    qnEventRuleConnector->at_remoteArchiveSyncFinished(resource);
    return true;
}

std::vector<RemoteArchiveEntry> SynchronizationTask::filterEntries(
    const QnSecurityCamResourcePtr& resource,
    const std::vector<RemoteArchiveEntry>& allEntries)
{
    NX_ASSERT(
        qnNormalStorageMan,
        lit("Storage manager should be initialized before archive synchronizer"));

    auto uniqueId = resource->getUniqueId();
    auto catalog = qnNormalStorageMan->getFileCatalog(
        uniqueId,
        DeviceFileCatalog::prefixByCatalog(QnServer::ChunksCatalog::HiQualityCatalog));

    auto serverDb = QnServerDb::instance();
    if (!serverDb)
        return std::vector<RemoteArchiveEntry>();

    auto lastSyncTimeMs = serverDb->getLastRemoteArchiveSyncTimeMs(resource);

    std::vector<RemoteArchiveEntry> filtered;
    for (const auto& entry : allEntries)
    {
        if (m_canceled)
            return filtered;

        bool needSyncEntry = (lastSyncTimeMs < (int64_t)(entry.startTimeMs + entry.durationMs))
            && !catalog->containTime(entry.startTimeMs + entry.durationMs / 2);

        qDebug() << "ENTRY NEED TO BE SYNCHRONIZED:" << needSyncEntry
            << "(ENTRY ID)" << entry.id
            << "(CONTAIN TIME)" << catalog->containTime(entry.startTimeMs + entry.durationMs / 2)
            << "(LASTSYNC TIME)" << lastSyncTimeMs
            << "(ENTRY END TIME)" << entry.startTimeMs + entry.durationMs
            << "(MIDDLE)" << entry.startTimeMs + entry.durationMs / 2
            << "(DURATION)" << entry.durationMs;

        if (needSyncEntry)
        {
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

bool SynchronizationTask::copyEntryToArchive(
    const QnSecurityCamResourcePtr& resource,
    const RemoteArchiveEntry& entry)
{
    if (m_canceled)
        return false;

    auto serverDb = QnServerDb::instance();
    if (!serverDb)
        return serverDb;

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

    if (buffer.size() < 1000)
        qDebug() << "!!!! Buffer too small. Buffer:" << buffer << "String:" << QString::fromUtf8(buffer); 

    int64_t realDurationMs = 0;
    result = writeEntry(resource, entry, &buffer, &realDurationMs);
    if (result)
        serverDb->updateLastRemoteArchiveSyncTimeMs(resource, entry.startTimeMs + realDurationMs);

    return result;
}

bool SynchronizationTask::writeEntry(
    const QnSecurityCamResourcePtr& resource,
    const RemoteArchiveEntry& entry,
    BufferType* buffer,
    int64_t* outRealDurationMs)
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

    const int64_t kEpsilonMs = MAX_FRAME_DURATION;
    int64_t realDurationMs = std::round(helper.durationMs());

    int64_t diff = entry.durationMs - realDurationMs;
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
    if (catalog->containTime(entry.startTimeMs)
        && catalog->containTime(entry.startTimeMs + realDurationMs / 2)
        && catalog->containTime(entry.startTimeMs + realDurationMs))
    {
        return true;
    }

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
        needMotion,
        &realDurationMs);

    if (!succesfulWrite)
        return false; //< Should we call fileFinished?

    qDebug() << "File " << fileNameWithExtension << "has been written";

    if (outRealDurationMs)
        *outRealDurationMs = realDurationMs;

    diff = entry.durationMs - realDurationMs;
    if (diff < 0)
        diff = -diff;

    if (realDurationMs == AV_NOPTS_VALUE || diff <= kEpsilonMs)
        realDurationMs = entry.durationMs;

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
    bool needMotion,
    int64_t* outRealDurationMs)
{
    qDebug() << "Converting and writing file" << fileName;
    NX_LOGX(lm("Converting and writing file %1").arg(fileName), cl_logDEBUG1);

    QBuffer* ioDevice = new QBuffer(); //< Will be freed by the owning storage.
    ioDevice->setBuffer(buffer);
    ioDevice->open(QIODevice::ReadOnly);

    const QString temporaryFilePath = QString::number(nx::utils::random::number());
    QnResourcePtr storageResource(new DummyResource());
    storageResource->setUrl(temporaryFilePath);

    QnExtIODeviceStorageResourcePtr storage(new QnExtIODeviceStorageResource(m_commonModule));
    storage->registerResourceData(temporaryFilePath, ioDevice);

    using namespace  nx::mediaserver_core::plugins;
    std::shared_ptr<QnAviArchiveDelegate> reader(needMotion
        ? new AviMotionArchiveDelegate()
        : new QnAviArchiveDelegate());

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

    while (auto mediaData = reader->getNextData())
    {
        if (m_canceled)
            break;

        if (mediaData->dataType == QnAbstractMediaData::EMPTY_DATA)
            break;


        recorder.processData(mediaData);
    }

    qDebug() << "=======> Duration of the chunk!" << recorder.duration();
    if (outRealDurationMs)
        *outRealDurationMs = recorder.duration() / 1000;

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

