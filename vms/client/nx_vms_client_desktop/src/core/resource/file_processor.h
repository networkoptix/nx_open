#pragma once

#include <QtCore/QList>
#include <QtCore/QUrl>

#include <core/resource/resource_fwd.h>

class QnFileProcessor
{
public:
    static QStringList findAcceptedFiles(const QStringList &files);
    static inline QStringList findAcceptedFiles(const QString &path)
    { return QnFileProcessor::findAcceptedFiles(QStringList() << path); }

    static QStringList findAcceptedFiles(const QList<QUrl> &urls);
    static inline QStringList findAcceptedFiles(const QUrl &url)
    { return QnFileProcessor::findAcceptedFiles(QStringList() << url.toLocalFile()); }

    static QnResourcePtr createResourcesForFile(const QString& fileName);
    static QnResourceList createResourcesForFiles(const QStringList &files);

    /**
     * Function creates a set of resources for the given filepaths. If resource pool is provided,
     * existing resources can be returned. Resources are not added to the pool automatically.
     */
    static QnResourceList findOrCreateResourcesForFiles(
        const QList<QUrl>& urls,
        QnResourcePool* resourcePool);

    static void deleteLocalResources(const QnResourceList &resources);
};
