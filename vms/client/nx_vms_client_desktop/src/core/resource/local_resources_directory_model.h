#pragma once

#include <QtCore/QFileSystemWatcher>
#include <QtCore/QTimer>

namespace nx::vms::client::desktop {

class QnLocalResourcesDirectoryModel : public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    QnLocalResourcesDirectoryModel(QObject* parent = nullptr);

    QStringList getLocalResourcesDirectories() const;
    void setLocalResourcesDirectories(const QStringList& paths);

    QStringList getAllFilePaths() const;
    QStringList getFilePaths(const QString& directoryPath);

signals:
    void filesAdded(const QStringList& newFiles);

private:
    void addWatchedDirectory(const QString& path);
    void removeWatchedDirectory(const QString& path);
    void onDirectoryChanged(const QString& path);
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
