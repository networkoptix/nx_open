#include "resource_directory_browser.h"

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QThread>

#include <common/common_module.h>

#include <core/resource/avi/avi_resource.h>
#include <core/resource/avi/filetypesupport.h>
#include <core/resource/layout_reader.h>
#include <core/resource/file_layout_resource.h>
#include <core/resource_management/resource_pool.h>

#include <client_core/client_core_module.h>

#include <nx/core/layout/layout_file_info.h>
#include <nx/fusion/model_functions.h>
#include <nx/utils/log/log.h>

#include <nx_ec/data/api_conversion_functions.h>

namespace {

static const int kMaxResourceCount = 1024;
static const int kUnlimited = -1;

QString fixSeparators(const QString& filePath)
{
    return QDir::fromNativeSeparators(filePath);
}

}

QnResourceDirectoryBrowser::QnResourceDirectoryBrowser(QObject* parent):
    QnAbstractResourceSearcher(qnClientCoreModule->commonModule()),
    QObject(parent),
    QnAbstractFileResourceSearcher(qnClientCoreModule->commonModule())
{
    connect(&m_fsWatcher, &QFileSystemWatcher::directoryChanged,
        this, &QnResourceDirectoryBrowser::at_filesystemDirectoryChanged);

    connect(this, &QnResourceDirectoryBrowser::startLocalDiscovery,
        this, &QnResourceDirectoryBrowser::at_startLocalDiscovery, Qt::QueuedConnection);

    connect(this, &QnResourceDirectoryBrowser::trackResources,
        this, &QnResourceDirectoryBrowser::at_trackResources, Qt::QueuedConnection);
}

QnResourcePtr QnResourceDirectoryBrowser::createResource(const QnUuid& resourceTypeId,
    const QnResourceParams& params)
{
    QnResourcePtr result;

    if (!isResourceTypeSupported(resourceTypeId))
        return result;

    auto resourcePool = qnClientCoreModule->commonModule()->resourcePool();
    result = createArchiveResource(params.url, resourcePool);
    result->setTypeId(resourceTypeId);

    return result;
}

QString QnResourceDirectoryBrowser::manufacturer() const
{
    return lit("DirectoryBrowser");
}

void QnResourceDirectoryBrowser::cleanup()
{
    m_resourceReady = false;
}

QnResourceList QnResourceDirectoryBrowser::findResources()
{
#if 0 // Turned off to make resource discovery work after application has started.
    if (m_resourceReady)
        return QnResourceList();
    setDiscoveryMode( DiscoveryMode::disabled );
#endif // if 0

    ResourceCache cache;
    {
        QnMutexLocker lock(&m_cacheMutex);
        cache = this->m_resourceCache;
    }

    QThread::currentThread()->setPriority(QThread::IdlePriority);

    NX_DEBUG(this, lit("Browsing directories..."));

    QTime time;
    time.restart();

    QnResourceList result;

    QStringList dirs = getPathCheckList();

    int found = 0;

    // List of paths to be tracked by FS watcher. Contains both files and directories.
    QStringList paths;
    // Handler creates resource for discovered file and adds it to resource pool.
    BrowseHandler handler = makeDiscoveryHandler(result, paths);

    // Search through directory list.
    for (const QString& dir: dirs)
    {
        int limit = std::max(kMaxResourceCount - found, 0);
        if (kMaxResourceCount < 0)
            limit = kUnlimited;
        found += findResources(dir, cache, handler, limit);
        if (shouldStop())
            break;
    }

    NX_DEBUG(this, lit("Done(Browsing directories). Time elapsed = %1.").arg(time.elapsed()));

    QThread::currentThread()->setPriority(QThread::LowPriority);

    m_resourceReady = true;

    if (!result.empty())
    {
        QGenericReturnArgument ret;
        emit trackResources(result, paths);
    }

    return result;
}

void QnResourceDirectoryBrowser::at_startLocalDiscovery()
{
    resourcePool()->addResources(findResources());
}

QnResourceDirectoryBrowser::BrowseHandler QnResourceDirectoryBrowser::makeDiscoveryHandler(
    QnResourceList& result, QStringList& paths)
{
    return
        [this, &result, &paths](const QString& path, bool isDir) -> bool
        {
            paths.append(path);

            if (!isDir)
            {
                QnResourcePtr res = createArchiveResource(path, resourcePool());
                if (!res)
                    return false;
                if (res->getId().isNull() && !res->getUniqueId().isEmpty())
                    res->setId(guidFromArbitraryData(res->getUniqueId().toUtf8())); //< Create same IDs for the same files.
                result.append(res);
            }
            return true;
        };
}

QnResourcePtr QnResourceDirectoryBrowser::checkFile(const QString& filename) const
{
    auto resourcePool = qnClientCoreModule->commonModule()->resourcePool();
    return resourceFromFile(filename, resourcePool); // TODO: #GDM #3.1 refactor all the scheme. Adding must not be here
}

// =============================================================================================
int QnResourceDirectoryBrowser::findResources(
    const QString& directory,
    const ResourceCache& cache,
    BrowseHandler handler,
    int maxResources)
{
    // Note: This method can be called from a separate thread. Beware!
    NX_DEBUG(this, lit("Checking %1").arg(directory));

    if (shouldStop() || maxResources == 0)
        return 0;

    int resourcesFound = 0;

    // We should gather discovered directories to a separate list and then push it to FS watcher
    handler(directory, true);

    const QList<QFileInfo> files = QDir(directory).entryInfoList(
        QDir::Dirs | QDir::NoDotAndDotDot | QDir::Files | QDir::NoSymLinks);
    for (const auto& file: files)
    {
        if (shouldStop())
            break;

        if (maxResources != kUnlimited && maxResources <= resourcesFound)
            break;

        QString absoluteFilePath = file.absoluteFilePath();
        if (file.isDir())
        {
            if (absoluteFilePath != directory)
            {
                auto remainingResourceCount = std::max<int>(maxResources - resourcesFound, 0);
                resourcesFound +=
                    findResources(absoluteFilePath, cache, handler, remainingResourceCount);
            }
        }
        else
        {
            if (cache.contains(absoluteFilePath))
                continue;

            if (handler(absoluteFilePath, false))
                resourcesFound++;
        }
    }

    return resourcesFound;
}

QnFileLayoutResourcePtr QnResourceDirectoryBrowser::layoutFromFile(const QString& filename,
    QnResourcePool* resourcePool)
{
    const QString layoutUrl = fixSeparators(filename);
    NX_ASSERT(layoutUrl == filename);

    // Check the file still seems to be valid.
    const auto fileInfo = nx::core::layout::identifyFile(layoutUrl);
    if (!fileInfo.isValid)
        return QnFileLayoutResourcePtr();

    // Check that the layout does not exist yet.
    QnUuid layoutId = guidFromArbitraryData(layoutUrl);
    if (resourcePool)
    {
        auto existingLayout = resourcePool->getResourceById<QnFileLayoutResource>(layoutId);
        if (existingLayout)
            return existingLayout;
    }

    // Read layout from file.
    return nx::vms::client::desktop::layout::layoutFromFile(layoutUrl);
}

QnResourcePtr QnResourceDirectoryBrowser::resourceFromFile(const QString& filename,
    QnResourcePool* resourcePool)
{
    const QString path = fixSeparators(filename);
    NX_ASSERT(path == filename);

    if (resourcePool)
    {
        if (const auto& existing = resourcePool->getResourceByUrl(path))
            return existing;
    }

    if (!QFileInfo::exists(path))
        return QnResourcePtr();

    return createArchiveResource(path, resourcePool);
}

QnResourcePtr QnResourceDirectoryBrowser::createArchiveResource(const QString& filename,
    QnResourcePool* resourcePool)
{
    const QString path = fixSeparators(filename);
    NX_ASSERT(path == filename);

    if (FileTypeSupport::isMovieFileExt(path))
        return QnResourcePtr(new QnAviResource(path, qnClientCoreModule->commonModule()));

    if (FileTypeSupport::isImageFileExt(path))
    {
        QnResourcePtr res =
            QnResourcePtr(new QnAviResource(path, qnClientCoreModule->commonModule()));
        res->addFlags(Qn::still_image);
        res->removeFlags(Qn::video | Qn::audio);
        return res;
    }

    if (FileTypeSupport::isValidLayoutFile(path))
        return layoutFromFile(path, resourcePool);

    return QnResourcePtr();
}

void QnResourceDirectoryBrowser::at_filesystemDirectoryChanged(const QString& path)
{
    QStringList files;

    ResourceCache cache;
    {
        QnMutexLocker lock(&m_cacheMutex);
        cache = m_resourceCache;
    }

    QnResourceList result;

    // Handler creates resource for discovered file and adds it to resource pool.
    BrowseHandler handler = makeDiscoveryHandler(result, files);

    if (findResources(path, cache, handler, kMaxResourceCount) > 0)
    {
        resourcePool()->addResources(result);
        trackResources(result, files);
    }
}

void QnResourceDirectoryBrowser::at_trackResources(const QnResourceList& resources,
    const QStringList& paths)
{
    QnMutexLocker lock(&m_cacheMutex);
    for (const auto& ptr: resources)
    {
        const QString& url = ptr->getUrl();
        if (!m_resourceCache.contains(url))
            m_resourceCache.insert(url, ptr);
    }
    // Add directories to watch. It properly handles duplicate paths.
    for (auto path: paths)
    {
        if (QFileInfo(path).isDir())
            m_fsWatcher.addPath(path);
    }
}

void QnResourceDirectoryBrowser::setPathCheckList(const QStringList& paths)
{
    QnMutexLocker lock(&m_mutex);
    // 1. Calculate which directories are removed.
    for (QString dir: m_pathListToCheck)
    {
        if (!paths.contains(dir))
            dropResourcesFromDirectory(dir);
    }
    // 2. Remove these directories.
    m_pathListToCheck = paths;
}

void QnResourceDirectoryBrowser::dropResourcesFromDirectory(const QString& dir)
{
    m_cacheMutex.lock();
    ResourceCache cache = m_resourceCache;
    m_cacheMutex.unlock();
    // Getting normalized directory path.
    QString directory = QDir(dir).absolutePath();

    // Paths to be removed from fs watcher.
    QStringList paths;
    // Resources to be removed
    QnResourceList resources;

    // Finding all the resources in specified directory.
    for (auto& resource: cache)
    {
        if (!resource->hasFlags(Qn::local))
        {
            continue;
        }

        QString path = resource->getUniqueId();
        // We need normalized path to do proper comparison.
        QString normPath = QFileInfo(path).absoluteFilePath();
        if (normPath.startsWith(directory))
        {
            paths.append(path);
            resources.append(resource);
        }
    }

    // Removing files from resource pool.
    QnResourcePool* pool = resourcePool();
    pool->removeResources(resources);

    // Removing files from FS watcher and cache.
    QnMutexLocker lock(&m_cacheMutex);
    for (QString path: paths)
        m_resourceCache.remove(path);
    m_fsWatcher.removePaths(paths);
}
