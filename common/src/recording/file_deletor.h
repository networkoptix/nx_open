#ifndef __FILE_DELETOR_H__
#define __FILE_DELETOR_H__

#include <QFile>
#include <QString>
#include <QMutex>
#include <QStringList>

class QnFileDeletor
{
public:
    void init(const QString& tmpRoot);
    static QnFileDeletor* instance();
    void deleteFile(const QString& fileName);
    
    QnFileDeletor();
private:
    void processPostponedFiles();
    void postponeFile(const QString& fileName);
    bool internalDeleteFile(const QString& fileName);
private:
    mutable QMutex m_mutex;
    QString m_mediaRoot;
    QStringList m_postponedFiles;
    QFile m_deleteCatalog;
    bool m_firstTime;
};

#define qnFileDeletor QnFileDeletor::instance()

#endif // __FILE_DELETOR_H__
