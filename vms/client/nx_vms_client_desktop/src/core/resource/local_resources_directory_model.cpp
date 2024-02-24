// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "local_resources_directory_model.h"

#include <QtCore/QDir>
#include <QtCore/QSet>

#include <client_core/client_core_module.h>
#include <core/resource/avi/avi_resource.h>
#include <core/resource/avi/filetypesupport.h>
#include <core/resource/file_layout_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/qt_helpers.h>
#include <nx/vms/common/system_context.h>

using namespace std::literals::chrono_literals;

namespace {

QString getResourceFilePath(const QnResourcePtr& resource)
{
    if (auto aviResource = resource.dynamicCast<QnAviResource>())
    {
        // Do not return path to embedded videos, they are governed by layouts.
        return aviResource->isEmbedded() ? QString() : aviResource->getUrl();
    }
    if (auto fileLayoutResource = resource.dynamicCast<QnFileLayoutResource>())
    {
        return fileLayoutResource->getUrl();
    }
    return QString();
}

}

namespace nx::vms::client::desktop {

LocalResourcesDirectoryModel::LocalResourcesDirectoryModel(QObject* parent):
    base_type(parent)
{
    m_deferredDirectoryChangeHandlerTimer.setSingleShot(true);
    m_deferredFileChangeHandlerTimer.setSingleShot(true);

    connect(&m_fileSystemWatcher, &QFileSystemWatcher::directoryChanged,
        this, &LocalResourcesDirectoryModel::onDirectoryChanged);

    connect(&m_fileSystemWatcher, &QFileSystemWatcher::fileChanged,
        this, &LocalResourcesDirectoryModel::onFileChanged);

    connect(&m_deferredDirectoryChangeHandlerTimer, &QTimer::timeout,
        this, &LocalResourcesDirectoryModel::processPendingDirectoryChanges);

    connect(&m_deferredFileChangeHandlerTimer, &QTimer::timeout,
        this, &LocalResourcesDirectoryModel::processPendingFileChanges);

    auto resourcePool = qnClientCoreModule->resourcePool();

    connect(resourcePool, &QnResourcePool::resourcesAdded, this,
        [this](const QnResourceList& resources)
        {
            for (const auto& resource: resources)
            {
                if (!resource->hasFlags(Qn::local))
                    continue;

                const auto path = getResourceFilePath(resource);
                if (FileTypeSupport::isMovieFileExt(path)
                    || FileTypeSupport::isValidLayoutFile(path))
                {
                    m_fileSystemWatcher.addPath(path);
                }
            }
        });

    connect(resourcePool, &QnResourcePool::resourcesRemoved, this,
        [this](const QnResourceList& resources)
        {
            for (const auto& resource: resources)
            {
                if (!resource->hasFlags(Qn::local))
                    continue;

                const auto path = getResourceFilePath(resource);
                if (!path.isNull())
                    m_fileSystemWatcher.removePath(path);
            }
        });
}

LocalResourcesDirectoryModel::~LocalResourcesDirectoryModel()
{
    m_deferredDirectoryChangeHandlerTimer.stop();
    m_deferredFileChangeHandlerTimer.stop();
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

    using namespace nx::utils;

    const QStringList newDirectories =
        toQSet(cleanedPaths).subtract(toQSet(m_localResourcesDirectories)).values();

    const QStringList removedDirectories =
        toQSet(m_localResourcesDirectories).subtract(toQSet(cleanedPaths)).values();

    m_localResourcesDirectories = cleanedPaths;

    for (const auto& newDirectory: newDirectories)
        addWatchedDirectory(newDirectory);

    for (const auto& removedDirectory: removedDirectories)
        removeWatchedDirectory(removedDirectory);
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

    for (const auto& childDirectory: childDirectories)
        addWatchedDirectory(dir.absoluteFilePath(childDirectory));

    for (auto& childFile: childFiles)
        childFile = dir.absoluteFilePath(childFile);

    NX_ASSERT_HEAVY_CONDITION(std::all_of(childFiles.cbegin(), childFiles.cend(),
        [](const QString& filePath) { return QFileInfo::exists(filePath); }));

    if (!childFiles.empty())
        emit filesAdded(childFiles);
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
    static constexpr auto kDelay = 100ms;

    m_pendingDirectoryChanges.insert(directoryPath);
    m_deferredDirectoryChangeHandlerTimer.start(kDelay);
}

void LocalResourcesDirectoryModel::onFileChanged(const QString& filePath)
{
    static constexpr auto kDelay = 250ms;

    m_pendingFileChanges.insert(filePath);
    m_deferredFileChangeHandlerTimer.start(kDelay);
}

void LocalResourcesDirectoryModel::processPendingDirectoryChanges()
{
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
            newChildFile = dir.absoluteFilePath(newChildFile);

        NX_ASSERT_HEAVY_CONDITION(std::all_of(newChildFiles.cbegin(), newChildFiles.cend(),
            [](const QString& filePath) { return QFileInfo::exists(filePath); }));

        if (!newChildFiles.empty())
            emit filesAdded(newChildFiles);
    }

    m_pendingDirectoryChanges.clear();
}

void LocalResourcesDirectoryModel::processPendingFileChanges()
{
    for (const QString& filePath: m_pendingFileChanges)
    {
        if (FileTypeSupport::isValidLayoutFile(filePath))
        {
            // QFileSystemWatcher loses tracking after temporary file renaming operation.
            m_fileSystemWatcher.addPath(filePath);
            emit layoutFileChanged(filePath);
        }
        else if (FileTypeSupport::isMovieFileExt(filePath))
        {
            emit videoFileChanged(filePath);
        }
    }
    m_pendingFileChanges.clear();
}

} // namespace nx::vms::client::desktop
