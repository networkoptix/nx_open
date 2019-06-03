#pragma once

#include <QtCore/QHash>
#include <QtCore/QFileSystemWatcher>
#include <QtCore/QTimer>

namespace nx::vms::client::desktop {

class LocalResourcesDirectoryModel : public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    LocalResourcesDirectoryModel(QObject* parent = nullptr);

    QStringList getLocalResourcesDirectories() const;
    void setLocalResourcesDirectories(const QStringList& paths);

    QStringList getAllFilePaths() const;
    QStringList getFilePaths(const QString& directoryPath);

signals:
    void filesAdded(const QStringList& newFiles);
    void layoutFileChanged(const QString& filePath);

private:
    void addWatchedDirectory(const QString& path);
    void removeWatchedDirectory(const QString& path);
    void onDirectoryChanged(const QString& path);
    void onFileChanged(const QString& path);
    void processPendingDirectoryChanges();

private:
    QFileSystemWatcher m_fileSystemWatcher;
    QTimer m_deferredDirectoryChangeHandlerTimer;
    QStringList m_pendingDirectoryChanges;

    QStringList m_localResourcesDirectories;
    QHash<QString, QStringList> m_childFiles;
    QHash<QString, QStringList> m_childDirectories;
};

} // namespace nx::vms::client::desktop
