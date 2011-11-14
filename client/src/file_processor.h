#ifndef QN_FILE_PROCESSOR_H
#define QN_FILE_PROCESSOR_H

#include <QStringList>

class QnFileProcessor {
public:
    /**
     * \param path                      Mask that is used for file search.
     * \param[out] list                 List that found files will be appended to.
     */
    static void findAcceptedFiles(const QString &path, QStringList *list);
};

#endif // QN_FILE_PROCESSOR_H
