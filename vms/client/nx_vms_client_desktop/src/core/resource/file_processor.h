// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QList>
#include <QtCore/QUrl>

#include <core/resource/resource_fwd.h>

class QnFileProcessor
{
public:
    static QStringList findAcceptedFiles(const QStringList &files);
    static QStringList findAcceptedFiles(const QString &path)
    { return QnFileProcessor::findAcceptedFiles(QStringList() << path); }

    static QStringList findAcceptedFiles(const QList<QUrl> &urls);
    static QStringList findAcceptedFiles(const QUrl &url)
    { return QnFileProcessor::findAcceptedFiles(QStringList() << url.toLocalFile()); }

    /** Find existing resource for the given filename. */
    static QnResourcePtr findResourceForFile(
        const QString& fileName,
        QnResourcePool* resourcePool);

    static QnResourcePtr createResourcesForFile(
        const QString& fileName,
        QnResourcePool* resourcePool);
    static QnResourceList createResourcesForFiles(
        const QStringList& files,
        QnResourcePool* resourcePool);

    /**
     * Function creates a set of resources for the given filepaths. If resource pool is provided,
     * existing resources can be returned. Resources are not added to the pool automatically.
     */
    static QnResourceList findOrCreateResourcesForFiles(
        const QList<QUrl>& urls,
        QnResourcePool* resourcePool);

    static void deleteLocalResources(const QnResourceList& resources);
};
