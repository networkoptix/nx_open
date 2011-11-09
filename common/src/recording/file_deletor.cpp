#include "file_deletor.h"
#include "utils/common/log.h"
#include "utils/common/util.h"

QnFileDeletor* fileDeletor = 0;
QnFileDeletor* QnFileDeletor::instance()
{
    return fileDeletor;
}

QnFileDeletor::QnFileDeletor(const QString& tmpRoot)
{
    Q_ASSERT( fileDeletor == 0);
    fileDeletor = this;
    m_firstTime = true;
    m_mediaRoot = closeDirPath(tmpRoot);
    m_deleteCatalog.setFileName(m_mediaRoot +  "delete_latter.csv");

}

void QnFileDeletor::deleteFile(const QString& fileName)
{
    processPostponedFiles();
    if (QFile::exists(fileName) && !QFile::remove(fileName))
    {
        cl_log.log("Can't delete file right now. Postpone deleting. Name=", fileName, cl_logWARNING);
        postponeFile(fileName);
    }
}

void QnFileDeletor::postponeFile(const QString& fileName)
{
    QMutexLocker lock(&m_mutex);
    m_postponedFiles.append(fileName);
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
    QMutexLocker lock(&m_mutex);
    if (m_firstTime)
    {
        // read postpone file
        if (m_deleteCatalog.open(QFile::ReadOnly))
        {
            QByteArray line = m_deleteCatalog.readLine().trimmed();
            while(!line.isEmpty())
            {
                m_postponedFiles.append(QString::fromUtf8(line));
                line = m_deleteCatalog.readLine().trimmed();
            }
            m_deleteCatalog.close();
        }
        m_firstTime = false;
    }

    if (m_postponedFiles.isEmpty())
        return;

    QStringList newList;
    for (QStringList::Iterator itr = m_postponedFiles.begin(); itr != m_postponedFiles.end(); ++itr)
    {
        if (QFile::exists(*itr) && !QFile::remove(*itr))
            newList << *itr;
    }
    if (newList.isEmpty())
    {
        m_deleteCatalog.close();
        if (QFile::remove(m_deleteCatalog.fileName()))
            m_postponedFiles.clear();
        return;
    }

    QFile tmpFile(m_mediaRoot + "tmp.csv");
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
