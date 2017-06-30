#ifndef QN_FILE_PROCESSOR_H
#define QN_FILE_PROCESSOR_H

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

    static void deleteLocalResources(const QnResourceList &resources);
};

#endif // QN_FILE_PROCESSOR_H
