#include "local_resources_directory_model.h"

#include <QtCore/QSet>
#include <QtCore/QDir>

#include <core/resource/avi/filetypesupport.h>

using namespace std::literals::chrono_literals;

namespace nx::vms::client::desktop {

LocalResourcesDirectoryModel::LocalResourcesDirectoryModel(QObject* parent):
    base_type(parent)
{
    m_deferredDirectoryChangeHandlerTimer.setSingleShot(true);

    connect(&m_fileSystemWatcher, &QFileSystemWatcher::directoryChanged,
        this, &LocalResourcesDirectoryModel::onDirectoryChanged);

    connect(&m_fileSystemWatcher, &QFileSystemWatcher::fileChanged,
        this, &LocalResourcesDirectoryModel::onFileChanged);

    connect(&m_deferredDirectoryChangeHandlerTimer, &QTimer::timeout,
        this, &LocalResourcesDirectoryModel::processPendingDirectoryChanges);
}

QStringList LocalResourcesDirectoryModel::getLocalResourcesDirectories() const
{
    return m_localResourcesDirectories;
}

void LocalResourcesDirectoryModel::setLocalResourcesDirectories(const QStringList& paths)
{
    QStringList cleanedPaths;
    for (const auto& path: paths)
    {
        const auto canonicalPath = QDir(path).canonicalPath();
        if (!canonicalPath.isEmpty())
            cleanedPaths.append(canonicalPath);
    }

    const QStringList newDirectories =
        cleanedPaths.toSet().subtract(m_localResourcesDirectories.toSet()).toList();

    const QStringList removedDirectories =
        m_localResourcesDirectories.toSet().subtract(cleanedPaths.toSet()).toList();

    m_localResourcesDirectories = cleanedPaths;

    for (const auto& newDirectory: newDirectories)
        addWatchedDirectory(newDirectory);

    for (const auto& removedDirectory: removedDirectories)
        removeWatchedDirectory(removedDirectory);
}

QStringList LocalResourcesDirectoryModel::getAllFilePaths() const
{
    QStringList result;
    for (auto i = m_childFiles.cbegin(); i != m_childFiles.cend(); ++i)
    {
        for (const auto& filename: i.value())
            result.append(QDir(i.key()).absoluteFilePath(filename));
    }
    return result;
}

QStringList LocalResourcesDirectoryModel::getFilePaths(const QString& directoryPath)
{
    QStringList result;
    for (auto i = m_childFiles.cbegin(); i != m_childFiles.cend(); ++i)
    {
        if (!QDir(i.key()).canonicalPath().startsWith(QDir(directoryPath).canonicalPath()))
            continue;
        for (const auto& filename: i.value())
            result.append(QDir(i.key()).absoluteFilePath(filename));
    }
    return result;
}

void LocalResourcesDirectoryModel::addWatchedDirectory(const QString& path)
{
    QDir dir(path);
    const auto canonicalPath = dir.canonicalPath();

    m_fileSystemWatcher.addPath(canonicalPath);

    auto childDirectories = dir.entryList(
        QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks, QDir::Name);

    auto childFiles = dir.entryList(
        QDir::Files | QDir::NoDotAndDotDot | QDir::NoSymLinks, QDir::Name);

    m_childFiles.insert(canonicalPath, childFiles);
    m_childDirectories.insert(canonicalPath, childDirectories);

    for (const auto& fileName: childFiles)
    {
        const auto filePath = dir.filePath(fileName);
        if (FileTypeSupport::isValidLayoutFile(filePath))
            m_fileSystemWatcher.addPath(filePath);
    }

    for (const auto& childDirectory: childDirectories)
        addWatchedDirectory(dir.absoluteFilePath(childDirectory));
}

void LocalResourcesDirectoryModel::removeWatchedDirectory(const QString& path)
{
    for (const auto& key: m_childFiles.keys())
    {
        if (key.startsWith(path))
            m_childFiles.remove(key);
    }

    for (const auto& key: m_childDirectories.keys())
    {
        if (key.startsWith(path))
        {
            m_childDirectories.remove(key);
            m_fileSystemWatcher.removePath(key);
        }
    }
}

void LocalResourcesDirectoryModel::onDirectoryChanged(const QString& directoryPath)
{
    const auto delay = 100ms;
    m_pendingDirectoryChanges.append(directoryPath);
    if (m_deferredDirectoryChangeHandlerTimer.isActive())
        m_deferredDirectoryChangeHandlerTimer.stop();
    m_deferredDirectoryChangeHandlerTimer.start(delay);
}

void LocalResourcesDirectoryModel::processPendingDirectoryChanges()
{
    m_pendingDirectoryChanges.removeDuplicates();

    for (const auto& directoryPath: m_pendingDirectoryChanges)
    {
        QDir dir(directoryPath);

        QStringList newChildDirectories;
        auto& previousChildDirectories = m_childDirectories[directoryPath];
        auto childDirectories = dir.entryList(
            QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks, QDir::Name);
        std::set_difference(childDirectories.cbegin(), childDirectories.cend(),
            previousChildDirectories.cbegin(), previousChildDirectories.cend(),
            std::back_inserter(newChildDirectories));

        auto& previousChildFiles = m_childFiles[directoryPath];
        auto childFiles = dir.entryList(
            QDir::Files | QDir::NoDotAndDotDot | QDir::NoSymLinks, QDir::Name);
        QStringList newChildFiles;
        std::set_difference(childFiles.cbegin(), childFiles.cend(),
            previousChildFiles.cbegin(), previousChildFiles.cend(),
            std::back_inserter(newChildFiles));

        previousChildDirectories = childDirectories;
        previousChildFiles = childFiles;

        for (const auto& newChildDirectory: newChildDirectories)
            addWatchedDirectory(dir.absoluteFilePath(newChildDirectory));

        for (auto& newChildFile: newChildFiles)
        {
            newChildFile = dir.absoluteFilePath(newChildFile);
            if (FileTypeSupport::isValidLayoutFile(newChildFile))
                m_fileSystemWatcher.addPath(newChildFile);
        }

        if (!newChildFiles.empty())
            emit filesAdded(newChildFiles);
    }

    m_pendingDirectoryChanges.clear();
}

void LocalResourcesDirectoryModel::onFileChanged(const QString& filePath)
{
    if (FileTypeSupport::isValidLayoutFile(filePath))
    {
        // QFileSystemWatcher lose tracking after temporary file renaming operation.
        m_fileSystemWatcher.addPath(filePath);
        emit layoutFileChanged(filePath);
    }
}

} // namespace nx::vms::client::desktop
