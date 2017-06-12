#ifndef __FILE_DELETOR_H__
#define __FILE_DELETOR_H__

#include <set>
#include <QtCore/QElapsedTimer>
#include <QtCore/QDir>
#include <QtCore/QQueue>
#include <QtCore/QFile>
#include <QtCore/QString>
#include <nx/utils/thread/mutex.h>
#include <QtCore/QStringList>
#include <QtCore/QThread>
#include <QtCore/QTimer>
#include "nx/utils/thread/long_runnable.h"
#include <common/common_module_aware.h>

class QnFileDeletor: public QnLongRunnable, public QnCommonModuleAware
{
    Q_OBJECT
public:
    void init(const QString& tmpRoot);
    static QnFileDeletor* instance();
    void deleteFile(const QString& fileName, const QnUuid &storageId);

    QnFileDeletor(QnCommonModule* commonModule);
    ~QnFileDeletor();

    virtual void run() override;
private:
    void processPostponedFiles();
    void postponeFile(const QString& fileName, const QnUuid &storageId);
    bool internalDeleteFile(const QString& fileName);
private:
    struct PostponedFileData
    {
        QString fileName;
        QnUuid storageId;

        PostponedFileData(const QString &fileName, const QnUuid &storageId)
            : fileName(fileName),
              storageId(storageId)
        {}
    };
    friend bool operator < (const PostponedFileData &lhs, const PostponedFileData &rhs);

    typedef QQueue<PostponedFileData> PostponedFileDataQueue;
    typedef std::set<PostponedFileData> PostponedFileDataSet;

    mutable QnMutex m_mutex;
    QString m_mediaRoot;
    PostponedFileDataSet m_postponedFiles;
    PostponedFileDataQueue m_newPostponedFiles;
    QFile m_deleteCatalog;
    bool m_firstTime;
    QElapsedTimer m_postponeTimer;
    QElapsedTimer m_storagesTimer;
};

inline bool operator < (const QnFileDeletor::PostponedFileData &lhs, const QnFileDeletor::PostponedFileData &rhs)
{
    return lhs.fileName < rhs.fileName ? true :
                                         rhs.fileName < lhs.fileName ? false :
                                                                       lhs.storageId < rhs.storageId;
}

#define qnFileDeletor QnFileDeletor::instance()

#endif // __FILE_DELETOR_H__
