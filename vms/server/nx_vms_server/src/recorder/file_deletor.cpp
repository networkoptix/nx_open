#include <stdio.h>
#include <nx/utils/log/log.h>
#include <nx/utils/random.h>
#include "utils/common/util.h"
#include "storage_manager.h"
#include <nx/utils/system_error.h>
#include <core/resource_management/resource_pool.h>
#include <media_server/media_server_module.h>
#include <nx/utils/url.h>

#include "file_deletor.h"

static const int POSTPONE_FILES_INTERVAL = 1000*60;
static const int SPACE_CLEARANCE_INTERVAL = 10;

QnFileDeletor::QnFileDeletor(QnMediaServerModule* serverModule):
    nx::vms::server::ServerModuleAware(serverModule)
{
}

QnFileDeletor::~QnFileDeletor()
{
    pleaseStop();
    wait();
}

void QnFileDeletor::run()
{
    initSystemThreadId();
    while (!m_needStop)
    {
        if (m_postponeTimer.elapsed() > POSTPONE_FILES_INTERVAL) {
            processPostponedFiles();
            m_postponeTimer.restart();
        }

        static const int DELTA = 5; // in range [-5..+5] seconds
        int thresholdSecs = nx::utils::random::numberDelta<int>(SPACE_CLEARANCE_INTERVAL, DELTA);
        if (serverModule()->backupStorageManager() && serverModule()->normalStorageManager() && m_storagesTimer.elapsed() > thresholdSecs * 1000)
        {
            m_storagesTimer.restart();
            serverModule()->normalStorageManager()->clearSpace();
            serverModule()->backupStorageManager()->clearSpace();
        }
        msleep(500);
    }
}

void QnFileDeletor::init(const QString& tmpRoot)
{
    m_firstTime = true;
    m_mediaRoot = closeDirPath(tmpRoot);
    m_deleteCatalog.setFileName(m_mediaRoot +  QLatin1String("delete_latter.csv"));

    m_postponeTimer.start();
    m_storagesTimer.start();
    start();
}


void QnFileDeletor::deleteFile(
    const QString& fileName, const QnUuid &storageId)
{
    NX_ASSERT(!storageId.isNull());
    auto storage = resourcePool()->getResourceById<QnStorageResource>(storageId);
    if (!storage)
        return; //< Nothing to delete any more.

    if (!storage->removeFile(fileName))
    {
        NX_DEBUG(this, lm("[Cleanup, FileStorage]. Postponing file %1").arg(fileName));
        postponeFile(fileName, storageId);
        return;
    }

    NX_VERBOSE(this, lm("[Cleanup, FileStorage]. File %1 removed successfully").arg(fileName));
}

void QnFileDeletor::postponeFile(const QString& fileName, const QnUuid &storageId)
{
    QnMutexLocker lock( &m_mutex );
    m_newPostponedFiles << PostponedFileData(fileName, storageId);
}

void QnFileDeletor::processPostponedFiles()
{

    if (m_firstTime)
    {
        // read postpone file
        if (m_deleteCatalog.open(QFile::ReadOnly))
        {
            QByteArray line = m_deleteCatalog.readLine().trimmed();
            while(!line.isEmpty())
            {
                if (line.indexOf(',') == -1) // old version <= 2.4 (fileName only)
                {
                    // Not supported any more. Just skip command
                }
                else // new version (fileName, storageUniqueId)
                {
                    auto splits = line.split(',');
                    m_postponedFiles.emplace(
                        QString::fromUtf8(splits[0].trimmed()),
                        QnUuid(splits[1].trimmed()));
                }
                line = m_deleteCatalog.readLine().trimmed();
            }
            m_deleteCatalog.close();
        }
        m_firstTime = false;
    }

    PostponedFileDataQueue newPostponedFiles;
    {
        QnMutexLocker lock( &m_mutex );
        newPostponedFiles = m_newPostponedFiles;
        m_newPostponedFiles.clear();
    }

    while (!newPostponedFiles.isEmpty())
    {
        PostponedFileData fileData = newPostponedFiles.dequeue();
        if (m_postponedFiles.find(fileData) != m_postponedFiles.cend())
            continue;
        m_postponedFiles.insert(fileData);
        if (!m_deleteCatalog.isOpen())
            m_deleteCatalog.open(QFile::WriteOnly | QFile::Append);
        QTextStream str(&m_deleteCatalog);
        str << fileData.fileName.toUtf8().data() << ',' << fileData.storageId.toByteArray() << "\n";
        str.flush();
    }

    if (m_postponedFiles.empty())
        return;

    PostponedFileDataSet newList;
    auto kMaxProcessPostponedDuration = std::chrono::seconds(5);
    auto start = std::chrono::steady_clock::now();

    for (PostponedFileDataSet::iterator itr = m_postponedFiles.begin(); itr != m_postponedFiles.end(); ++itr)
    {
        if (std::chrono::steady_clock::now() - start > kMaxProcessPostponedDuration)
        {
            NX_VERBOSE(this, lit("[Cleanup] process postponed files duration exceeded. Breaking."));
            for (; itr != m_postponedFiles.end(); ++itr)
                newList.insert(*itr);
            break;
        }

        auto storage = resourcePool()->getResourceById<QnStorageResource>(itr->storageId);
        bool needToPostpone = !storage || storage->getStatus() == Qn::ResourceStatus::Offline;

        if (!storage)
        {
            NX_VERBOSE(
                this, "[Cleanup] storage with id '%1' not found in pool. Postponing file '%2'",
                itr->storageId.toString(), nx::utils::url::hidePassword(itr->fileName));
        }
        else if (storage->getStatus() == Qn::ResourceStatus::Offline)
        {
            NX_VERBOSE(
                this, "[Cleanup] storage %1 is offline. Postponing file %2",
                nx::utils::url::hidePassword(storage->getUrl()),
                nx::utils::url::hidePassword(itr->fileName));
        }

        if (needToPostpone || !storage->removeFile(itr->fileName))
        {
            newList.insert(*itr);
            NX_VERBOSE(
                this, "[Cleanup] Postponing file %1. Reason: %2",
                nx::utils::url::hidePassword(itr->fileName),
                needToPostpone ? "Storage is offline or not in the resource pool" : "Delete failed");
        }
    }
    if (newList.empty())
    {
        m_deleteCatalog.close();
        if (QFile::remove(m_deleteCatalog.fileName()))
            m_postponedFiles.clear();
        return;
    }

    QFile tmpFile(m_mediaRoot + QLatin1String("tmp.csv"));
    if (!tmpFile.open(QFile::WriteOnly | QFile::Truncate))
        return;
    for(const auto& fileData: newList)
    {
        QTextStream str(&tmpFile);
        str << fileData.fileName.toUtf8().data() << ',' << fileData.storageId.toByteArray() << "\n";
        str.flush();
    }
    tmpFile.close();
    m_deleteCatalog.close();
    if (QFile::remove(m_deleteCatalog.fileName()))
    {
        if (tmpFile.rename(m_deleteCatalog.fileName()))
            m_postponedFiles = newList;
    }
}

qint64 QnFileDeletor::postponedFileSize(const QnUuid& storageId)
{
    QnMutexLocker lock(&m_mutex);
    qint64 result = 0;

    for (const auto& data: m_postponedFiles)
    {
        if (!storageId.isNull() && data.storageId != storageId)
            continue;
        if (!data.fileSize)
        {
            if (auto dataStorage = resourcePool()->getResourceById<QnStorageResource>(data.storageId))
            {
                // fileSize field is not key field, it can be changed.
                PostponedFileData& nonConstData = const_cast<PostponedFileData&>(data);
                nonConstData.fileSize = dataStorage->getFileSize(data.fileName);
            }
        }
        if (data.fileSize)
            result += *data.fileSize;
    }
    return result;
}
