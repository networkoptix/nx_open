#ifndef QN_FILE_PROCESSOR_H
#define QN_FILE_PROCESSOR_H

#include <QUrl>
#include <core/resource/resource.h>

class QnFileProcessor {
public:
    /**
     * \param path                      Mask that is used for file search.
     * \param[out] list                 List that found files will be appended to.
     */
    static void findAcceptedFiles(const QString &path, QStringList *list);

    static QStringList findAcceptedFiles(const QString &path);

    static void findAcceptedFiles(const QList<QUrl> &urls, QStringList *list);

    static QStringList findAcceptedFiles(const QList<QUrl> &urls);

    static QnResourceList creareResourcesForFiles(const QStringList &files);
};

#endif // QN_FILE_PROCESSOR_H
