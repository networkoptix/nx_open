#pragma once

#include <QtCore/QHash>
#include <QtCore/QSet>
#include <QtCore/QFileSystemWatcher>
#include <QtCore/QTimer>

namespace nx::vms::client::desktop {

class LocalResourcesDirectoryModel: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    LocalResourcesDirectoryModel(QObject* parent = nullptr);

    QStringList getLocalResourcesDirectories() const;
    void setLocalResourcesDirectories(const QStringList& paths);

signals:
    void filesAdded(const QStringList& newFiles);
    void layoutFileChanged(const QString& filePath);
    void videoFileChanged(const QString& filePath);

private:
    void addWatchedDirectory(const QString& path);
    void removeWatchedDirectory(const QString& path);
    void onDirectoryChanged(const QString& path);
    void onFileChanged(const QString& path);
    void processPendingDirectoryChanges();
    void processPendingFileChanges();

private:
    QFileSystemWatcher m_fileSystemWatcher;

    QTimer m_deferredDirectoryChangeHandlerTimer;
    QTimer m_deferredFileChangeHandlerTimer;

    QSet<QString> m_pendingDirectoryChanges;
    QSet<QString> m_pendingFileChanges;

    QStringList m_localResourcesDirectories;
    QHash<QString, QStringList> m_childFiles;
    QHash<QString, QStringList> m_childDirectories;
};

} // namespace nx::vms::client::desktop
