#include "device_file_catalog.h"
#include "storage_manager.h"
#include "utils/common/util.h"

DeviceFileCatalog::DeviceFileCatalog(QnResourcePtr resource): 
m_firstDeleteCount(0),
m_resource(resource)
{
    QString devTitleFile = qnStorage->storageRoots()[0] + QFileInfo(resource->getUrl()).baseName() + QString("/title.csv");
    m_file.setFileName(devTitleFile);
    QDir dir;
    dir.mkpath(QFileInfo(devTitleFile).absolutePath());
    if (!m_file.open(QFile::ReadWrite))
    {
        cl_log.log("Can't create title file ", devTitleFile, cl_logERROR);
        return;
    }

    if (m_file.size() == 0) 
    {
        QTextStream str(&m_file);
        str << "start; duration; storage; index\n"; // write CSV header
        str.flush();
    }
    else {
        deserializeTitleFile();
    }
}

bool DeviceFileCatalog::fileExists(const Chunk& chunk)
{
    QString prefix = qnStorage->storageRoots()[chunk.storageIndex] + QFileInfo(m_resource->getUrl()).baseName() + QString('/');


    QDateTime fileDate = QDateTime::fromMSecsSinceEpoch(chunk.startTime/1000);
    int currentParts[4];
    currentParts[0] = fileDate.date().year();
    currentParts[1] = fileDate.date().month();
    currentParts[2] = fileDate.date().day();
    currentParts[3] = fileDate.time().hour();
    QDir dir;
    bool sameDir = true;
    for (int i = 0; i < 4; ++i)
    {
        if (m_prevParts[i].first == currentParts[i])
        {
            // optimization. Check file existing without IO operation
            if (!m_prevParts[i].second)
                return false;
        }
        else 
        {
            sameDir = false;
            // new folder. check it
            for (int j = 0; j < i; ++j)
                prefix += strPadLeft(QString::number(currentParts[j]), 2, '0') + QString('/');

            bool exist = true;
            for (int j = i; j < 4; ++j)
            {
                prefix += strPadLeft(QString::number(currentParts[j]), 2, '0') + QString('/');
                if (exist)
                    exist &= dir.exists(prefix);
                m_prevParts[j].first = currentParts[j];
                m_prevParts[j].second = exist;
            }
            break;
        }
    }
    if (!m_prevParts[3].second)
        return false;
    if (!sameDir) {
        dir.cd(prefix);
        m_existFileList = dir.entryList(QDir::Files);
    }
    QString fName = strPadLeft(QString::number(chunk.fileIndex), 3, '0') + QString(".mkv");
    return m_existFileList.contains(fName);
}

void DeviceFileCatalog::deserializeTitleFile()
{
    QMutexLocker lock(&m_mutex);
    bool needRewriteFile = false;
    QFile newFile(m_file.fileName() + QString(".tmp"));
    bool useNewFile = newFile.open(QFile::WriteOnly);
    QTextStream str(&newFile);
    if (useNewFile)
        str << "start; duration; storage; index\n"; // write CSV header

    m_file.readLine(); // read header
    QByteArray line;
    do {
        line = m_file.readLine();
        QList<QByteArray> fields = line.split(';');
        if (fields.size() < 3)
            continue;
        qint64 startTime = fields[0].toLongLong();
        Chunk chunk(startTime, fields[1].toInt(), fields[2].toInt(), fields[3].trimmed().toInt());
        if (fileExists(chunk)) {
            ChunkMap::iterator itr = qUpperBound(m_chunks.begin(), m_chunks.end(), startTime);
            m_chunks.insert(itr, chunk);
            if (useNewFile)
                str << chunk.startTime << ';' << chunk.duration << ';' << chunk.storageIndex << ';' << chunk.fileIndex << '\n';
        }
        else {
            needRewriteFile = true;
        }
    } while (!line.isEmpty());
    str.flush();
    newFile.close();
    if (useNewFile && needRewriteFile) {
        m_file.close();
        newFile.close();
        if (m_file.remove())
            newFile.rename(m_file.fileName());
        m_file.open(QFile::ReadWrite);
        m_file.seek(m_file.size());
    }
}

void DeviceFileCatalog::addRecord(const Chunk& chunk)
{
    QMutexLocker lock(&m_mutex);

    ChunkMap::iterator itr = qUpperBound(m_chunks.begin(), m_chunks.end(), chunk.startTime);
    m_chunks.insert(itr, chunk);
    QTextStream str(&m_file);
    str << chunk.startTime << ';' << chunk.duration << ';' << chunk.storageIndex << ';' << chunk.fileIndex << '\n';
    str.flush();
}

void DeviceFileCatalog::deleteFirstRecord()
{
    QMutexLocker lock(&m_mutex);

    static const int DELETE_COEFF = 1000;

    if (m_firstDeleteCount < m_chunks.size())
        m_firstDeleteCount++;
    else {
        m_firstDeleteCount = 0;
        emit firstDataRemoved(m_chunks.size());
        m_chunks.clear();
    }
    if (m_firstDeleteCount == DELETE_COEFF) {
        emit firstDataRemoved(DELETE_COEFF);
        m_chunks.erase(m_chunks.begin(), m_chunks.begin() + DELETE_COEFF);
        m_firstDeleteCount = 0;
    }
}

int DeviceFileCatalog::findFileIndex(qint64 startTime) const
{
    QMutexLocker lock(&m_mutex);

    if (m_chunks.isEmpty())
        return -1;

    ChunkMap::const_iterator itr = qUpperBound(m_chunks.begin() + m_firstDeleteCount, m_chunks.end(), startTime);
    if (itr > m_chunks.begin())
        --itr;
    return itr - m_chunks.begin();
}

QString DeviceFileCatalog::fullFileName(const Chunk& chunk) const
{
    QMutexLocker lock(&m_mutex);
    return qnStorage->storageRoots()[chunk.storageIndex] + 
                QFileInfo(m_resource->getUrl()).baseName() + QString('/') +
                QnStorageManager::dateTimeStr(chunk.startTime) + 
                strPadLeft(QString::number(chunk.fileIndex), 3, '0') + 
                QString(".mkv");
}

qint64 DeviceFileCatalog::minTime() const
{
    QMutexLocker lock(&m_mutex);
    if (m_chunks.isEmpty())
        return AV_NOPTS_VALUE;
    else
        return m_chunks[0].startTime;
}

qint64 DeviceFileCatalog::maxTime() const
{
    QMutexLocker lock(&m_mutex);
    if (m_chunks.isEmpty())
        return AV_NOPTS_VALUE;
    else
        return m_chunks.last().startTime + m_chunks.last().duration;
}

DeviceFileCatalog::Chunk DeviceFileCatalog::chunkAt(int index) const
{
    QMutexLocker lock(&m_mutex);
    return m_chunks.at(index);
}

bool operator < (qint64 first, const DeviceFileCatalog::Chunk& other)
{ 
    return first < other.startTime; 
}

bool operator < (const DeviceFileCatalog::Chunk& first, qint64 other)
{ 
    return first.startTime < other; 
}

bool operator < (const DeviceFileCatalog::Chunk& first, const DeviceFileCatalog::Chunk& other) 
{ 
    return first.startTime < other.startTime; 
}
