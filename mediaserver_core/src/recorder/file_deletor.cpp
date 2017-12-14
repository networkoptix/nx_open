#include <cstdio>
#include "file_deletor.h"
#include <nx/utils/log/log.h>
#include <nx/utils/random.h>
#include "utils/common/util.h"
#include "storage_manager.h"
#include <nx/utils/system_error.h>
#include <core/resource_management/resource_pool.h>
#include <media_server/media_server_module.h>
#include <nx/mediaserver/root_tool.h>

static const int POSTPONE_FILES_INTERVAL = 1000*60;
static const int SPACE_CLEARANCE_INTERVAL = 10;

QnFileDeletor* QnFileDeletor_inst = 0;

QnFileDeletor::QnFileDeletor(QnCommonModule* commonModule):
    QnCommonModuleAware(commonModule)
{
    QnFileDeletor_inst = this;
}

QnFileDeletor::~QnFileDeletor()
{
    pleaseStop();
    wait();
    QnFileDeletor_inst = 0;
}

QnFileDeletor* QnFileDeletor::instance()
{
    return QnFileDeletor_inst;
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
        if (qnBackupStorageMan && qnNormalStorageMan && m_storagesTimer.elapsed() > thresholdSecs * 1000)
        {
            m_storagesTimer.restart();
            qnNormalStorageMan->clearSpace();
            qnBackupStorageMan->clearSpace();
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

bool QnFileDeletor::internalDeleteFile(const QString& fileName)
{
    if (std::remove(fileName.toLatin1().constData()))
        return true;

    if (const auto rootTool = qnServerModule->rootTool())
    {
        if (rootTool->changeOwner(fileName) && std::remove(fileName.toLatin1().constData()))
            return true;
    }

    auto lastErr = SystemError::getLastOSErrorCode();
    return lastErr == SystemError::fileNotFound || lastErr == SystemError::pathNotFound;
}

void QnFileDeletor::deleteFile(const QString& fileName, const QnUuid &storageId)
{
    if (!internalDeleteFile(fileName))
    {
        NX_LOG(lit("Cleanup. Can't delete file right now. Postpone deleting. Name=%1").arg(fileName), cl_logWARNING);
        postponeFile(fileName, storageId);
        return;
    }

    NX_LOG(lit("Cleanup. File %1 removed successfully.").arg(fileName), cl_logDEBUG1);
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
                if (line.indexOf(',') == -1) // old version (fileName)
                    m_postponedFiles.emplace(PostponedFileData(QString::fromUtf8(line), QnUuid()));
                else // new version (fileName, storageUniqueId)
                {
                    auto splits = line.split(',');
                    m_postponedFiles.emplace(QString::fromUtf8(splits[0].trimmed()), QnUuid::fromRfc4122(splits[1].trimmed()));
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
            NX_LOG(lit("[Cleanup] process postponed files duration exceeded. Breaking."), cl_logDEBUG2);
            for (; itr != m_postponedFiles.end(); ++itr)
                newList.insert(*itr);
            break;
        }

        if (itr->storageId.isNull()) // File from the old-style deleteCatalog. Try once and discard.
            internalDeleteFile(itr->fileName);
        else
        {
            auto storage = resourcePool()->getResourceById(itr->storageId);
            bool needToPostpone = !storage || storage->getStatus() == Qn::ResourceStatus::Offline;

            if (!storage)
            {
                NX_LOG(lit("[Cleanup] storage with id %1 not found in pool. Postponing file %2")
                        .arg(itr->storageId.toString())
                        .arg(itr->fileName), cl_logDEBUG2);
            }
            else if (storage->getStatus() == Qn::ResourceStatus::Offline)
            {
                NX_LOG(lit("[Cleanup] storage %1 is offline. Postponing file %2")
                        .arg(storage->getUrl())
                        .arg(itr->fileName), cl_logDEBUG2);
            }

            if (needToPostpone || !internalDeleteFile(itr->fileName))
            {
                newList.insert(*itr);
                NX_LOG(lit("[Cleanup] Postponing file %1. Reason: %2")
                    .arg(itr->fileName)
                    .arg(needToPostpone ? "Storage is offline or not in the resource pool" : "Delete failed"), cl_logDEBUG2);
            }
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
