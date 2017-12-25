#pragma once

#include "core/resource_management/resource_searcher.h"

class QnResourceDirectoryBrowser:
    public QObject,
    public QnAbstractFileResourceSearcher
{
    Q_OBJECT

    using base_type = QObject;
public:
    QnResourceDirectoryBrowser(QObject* parent = nullptr);

    virtual QnResourcePtr createResource(const QnUuid &resourceTypeId, const QnResourceParams& params) override;

    virtual QString manufacture() const override;
    virtual QnResourceList findResources() override;

    virtual void setPathCheckList(const QStringList& paths) override;

    virtual QnResourcePtr checkFile(const QString &filename) const override;

    void dropResourcesFromFolder(const QString &path);

    // Local files search only once. Use cleanup before search to re-search files again
    void cleanup();

    static QnLayoutResourcePtr layoutFromFile(const QString& filename, QnResourcePool* resourcePool);
    static QnResourcePtr resourceFromFile(const QString& filename, QnResourcePool* resourcePool);

protected:
    typedef QMap<QString, QnResourcePtr> ResourceCache;
    bool m_resourceReady{false};

    static QnResourcePtr createArchiveResource(const QString& filename, QnResourcePool* resourcePool);

    /**
     * Handler type for file discovery
     * @param path - path to discovered entity. It can be file or directory
     * @param isDir - flag that mentions whether we have a file or directory
     * @return bool
     */
    typedef std::function<bool(const QString& path, bool isDir)> BrowseHandler;

    /**
     * Recursive search for resources
     * @param directory - directory for the search
     * @param cache - current resource cache. Files, that are already in the cache are ignored
     * @param hadler - handler to be called for each discovered directort and file
     * @param maxResources - limit to number of resources to discover. Set to -1 to get all the resources
     * @return number of resources discovered
     */
    int findResources(const QString &directory, const ResourceCache & cache, BrowseHandler & handler, int maxResources);
protected slots:
    void at_filesystemDirectoryChanged(const QString& path);
    void at_filesystemFileChanged(const QString& path);
    void trackResources(const QnResourceList& resources, const QStringList& paths);
private:
    QFileSystemWatcher m_fsWatcher;

    // Local resource cache for fast mapping between file path and resource pointers
    ResourceCache m_resourceCache;
    mutable QnMutex m_cacheMutex;
};
