#pragma once

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
#include <nx/vms/server/server_module_aware.h>

namespace nx::vms::server { class RootTool; }

class QnFileDeletor: public QnLongRunnable, public nx::vms::server::ServerModuleAware
{
    Q_OBJECT
public:
    QnFileDeletor(QnMediaServerModule* serverModule);
    ~QnFileDeletor();

    void init(const QString& tmpRoot);
    void deleteFile(const QString& fileName, const QnUuid &storageId);

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
