#include "file_deletor.h"
#include "utils/common/log.h"
#include "utils/common/util.h"

Q_GLOBAL_STATIC (QnFileDeletor, inst);

QnFileDeletor* QnFileDeletor::instance()
{
    return inst();
}

QnFileDeletor::QnFileDeletor()
{
    m_postponeTimer.start();
    start();
}

QnFileDeletor::~QnFileDeletor()
{
    pleaseStop();
    wait();
}

void QnFileDeletor::run()
{
    while (!m_needStop)
    {
        if (m_postponeTimer.elapsed() > 1000*60) {
            processPostponedFiles();
            m_postponeTimer.restart();
        }

        DeleteInfo toDelete;
        m_mutex.lock();
        if (!m_toDeleteList.isEmpty())
            toDelete = m_toDeleteList.dequeue();
        m_mutex.unlock();

        if (!toDelete.name.isEmpty()) {
            if (toDelete.isDir)
            {
                QDir dir(toDelete.name);
                QList<QFileInfo> list = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
                foreach(const QFileInfo& fi, list)
                    checkAndDeleteFileInternal(fi.absoluteFilePath());
            }
            else {
                checkAndDeleteFileInternal(toDelete.name);
            }
        }
        else {
            msleep(1000);
        }
    }
}

void QnFileDeletor::checkAndDeleteFileInternal(const QString& fileName)
{
    if (QFile::exists(fileName))
    {
        if (!internalDeleteFile(fileName))
        {
            cl_log.log("Can't delete file right now. Postpone deleting. Name=", fileName, cl_logWARNING);
            postponeFile(fileName);
        }
    }
}

void QnFileDeletor::init(const QString& tmpRoot)
{
    m_firstTime = true;
    m_mediaRoot = closeDirPath(tmpRoot);
    m_deleteCatalog.setFileName(m_mediaRoot +  QLatin1String("delete_latter.csv"));
}

bool QnFileDeletor::internalDeleteFile(const QString& fileName)
{
    if (!QFile::remove(fileName))
        return false;
    QString dirName = fileName.left(fileName.lastIndexOf(QLatin1Char('/')));
    while(1) 
    {
        QDir dir (dirName);
        if (!dir.rmdir(dirName))
            break;
        dirName = dirName.left(dirName.lastIndexOf(QLatin1Char('/')));
    }
    return true;
}

void QnFileDeletor::deleteDir(const QString& dirName)
{
    QMutexLocker lock(&m_mutex);
    m_toDeleteList << DeleteInfo(dirName, true);
}

void QnFileDeletor::deleteFile(const QString& fileName)
{
    QMutexLocker lock(&m_mutex);
    m_toDeleteList << DeleteInfo(fileName, false);
}

void QnFileDeletor::postponeFile(const QString& fileName)
{
    if (m_postponedFiles.contains(fileName))
        return;
    m_postponedFiles << fileName;
    if (!m_deleteCatalog.isOpen())
    {
        m_deleteCatalog.open(QFile::WriteOnly | QFile::Append);
    }
    QTextStream str(&m_deleteCatalog);
    str << fileName.toUtf8().data() << "\n";
    str.flush();
}

void QnFileDeletor::processPostponedFiles()
{

    if (m_firstTime)
    {
        // read postpone file
        if (m_deleteCatalog.open(QFile::ReadOnly))
        {
            QByteArray line = m_deleteCatalog.readLine().trimmed();
            while(!line.isEmpty())
            {
                m_postponedFiles << QString::fromUtf8(line);
                line = m_deleteCatalog.readLine().trimmed();
            }
            m_deleteCatalog.close();
        }
        m_firstTime = false;
    }

    if (m_postponedFiles.isEmpty())
        return;

    QSet<QString> newList;
    for (QSet<QString>::Iterator itr = m_postponedFiles.begin(); itr != m_postponedFiles.end(); ++itr)
    {
        if (QFile::exists(*itr) && !internalDeleteFile(*itr))
            newList << *itr;
    }
    if (newList.isEmpty())
    {
        m_deleteCatalog.close();
        if (QFile::remove(m_deleteCatalog.fileName()))
            m_postponedFiles.clear();
        return;
    }

    QFile tmpFile(m_mediaRoot + QLatin1String("tmp.csv"));
    if (!tmpFile.open(QFile::WriteOnly | QFile::Truncate))
        return;
    foreach(QString fileName, newList)
    {
        tmpFile.write(fileName.toUtf8());
        tmpFile.write("\n");
    }
    tmpFile.close();
    m_deleteCatalog.close();
    if (QFile::remove(m_deleteCatalog.fileName()))
    {
        if (tmpFile.rename(m_deleteCatalog.fileName()))
            m_postponedFiles = newList;
    }
}
