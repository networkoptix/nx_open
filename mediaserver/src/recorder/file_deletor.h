#ifndef __FILE_DELETOR_H__
#define __FILE_DELETOR_H__

#include <QtCore/QElapsedTimer>
#include <QtCore/QDir>
#include <QtCore/QQueue>
#include <QtCore/QFile>
#include <QtCore/QString>
#include <utils/common/mutex.h>
#include <QtCore/QStringList>
#include <QtCore/QThread>
#include <QtCore/QTimer>
#include "utils/common/long_runnable.h"

class QnFileDeletor: public QnLongRunnable
{
public:
    void init(const QString& tmpRoot);
    static QnFileDeletor* instance();
    void deleteFile(const QString& fileName);
    void deleteDir(const QString& dirName);
    void deleteDirRecursive(const QString& dirName);
    
    QnFileDeletor();
    ~QnFileDeletor();

    virtual void run() override;
private:
    void processPostponedFiles();
    void postponeFile(const QString& fileName);
    bool internalDeleteFile(const QString& fileName);
private:

    mutable QnMutex m_mutex;
    QString m_mediaRoot;
    QSet<QString> m_postponedFiles;
    QQueue<QString> m_newPostponedFiles;
    QFile m_deleteCatalog;
    bool m_firstTime;
    QElapsedTimer m_postponeTimer;
    QElapsedTimer m_storagesTimer;
};

#define qnFileDeletor QnFileDeletor::instance()

#endif // __FILE_DELETOR_H__
