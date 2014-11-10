#include "file_deletor.h"
#include "utils/common/log.h"
#include "utils/common/util.h"
#include "storage_manager.h"

static const int POSTPONE_FILES_INTERVAL = 1000*60;
static const int SPACE_CLEARANCE_INTERVAL = 15 * 1000;

QnFileDeletor* QnFileDeletor_inst = 0;

QnFileDeletor::QnFileDeletor()
{
    m_postponeTimer.start();
    m_storagesTimer.start();
    start();
    QnFileDeletor_inst = this;
}

QnFileDeletor::~QnFileDeletor()
{
    pleaseStop();
    wait();
    QnFileDeletor_inst = 0;
}

QnFileDeletor* QnFileDeletor::instance()
{
    return QnFileDeletor_inst;
}

void QnFileDeletor::run()
{
    initSystemThreadId();
    while (!m_needStop)
    {
        if (m_postponeTimer.elapsed() > POSTPONE_FILES_INTERVAL) {
            processPostponedFiles();
            m_postponeTimer.restart();
        }
        
        if (qnStorageMan && m_storagesTimer.elapsed() > SPACE_CLEARANCE_INTERVAL)
        {
            qnStorageMan->clearSpace();
            m_storagesTimer.restart();
        }

        msleep(500);
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
    QString dirName = fileName.left(fileName.lastIndexOf(QDir::separator()));
    while(1) 
    {
        QDir dir (dirName);
        if (!dir.rmdir(dirName))
            break;
        dirName = dirName.left(dirName.lastIndexOf(QDir::separator()));
    }
    return true;
}

void QnFileDeletor::deleteDir(const QString& dirName)
{
    QDir dir(dirName);
    QList<QFileInfo> list = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
    for(const QFileInfo& fi: list)
        deleteFile(fi.absoluteFilePath());
}

void QnFileDeletor::deleteDirRecursive(const QString& dirName)
{
    QDir dir(dirName);
    QList<QFileInfo> list = dir.entryInfoList(QDir::Files |QDir::Dirs | QDir::NoDotAndDotDot);
    for(const QFileInfo& fi: list) {
        if (fi.isDir())
            deleteDirRecursive(fi.absoluteFilePath());
        else
            deleteFile(fi.absoluteFilePath());
    }
    dir.rmdir(dirName);
}

void QnFileDeletor::deleteFile(const QString& fileName)
{
    if (QFile::exists(fileName))
    {
        if (!internalDeleteFile(fileName))
        {
            NX_LOG(lit("Can't delete file right now. Postpone deleting. Name=%1").arg(fileName), cl_logWARNING);
            postponeFile(fileName);
        }
    }
}

void QnFileDeletor::postponeFile(const QString& fileName)
{
    QMutexLocker lock(&m_mutex);
    m_newPostponedFiles << fileName;
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

    QQueue<QString> newPostponedFiles;
    {
        QMutexLocker lock(&m_mutex);
        newPostponedFiles = m_newPostponedFiles;
        m_newPostponedFiles.clear();
    }

    while (!newPostponedFiles.isEmpty()) 
    {
        QString fileName = newPostponedFiles.dequeue();
        if (m_postponedFiles.contains(fileName))
            continue;
        m_postponedFiles << fileName;
        if (!m_deleteCatalog.isOpen())
            m_deleteCatalog.open(QFile::WriteOnly | QFile::Append);
        QTextStream str(&m_deleteCatalog);
        str << fileName.toUtf8().data() << "\n";
        str.flush();
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
    for(const QString& fileName: newList)
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
