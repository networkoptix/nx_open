#include <QMutex>
#include <QFileInfo>

#include "avi_dvd_strem_reader.h"
#include "device/device.h"
#include "base/dvd_reader/dvd_reader.h"
#include "base/dvd_reader/dvd_udf.h"
#include "base/dvd_reader/ifo_read.h"
#include "base/dvd_decrypt/dvdcss.h"

static const int IO_BLOCK_SIZE = 32 * 1024;
static const int IO_SKIP_BLOCK_SIZE = 1024 * 1024;

struct DvdDecryptInfo
{
    DvdDecryptInfo(): m_dvd_file(0) {}
    dvd_file_t* m_dvd_file;
};

CLAVIDvdStreamReader::CLAVIDvdStreamReader(CLDevice* dev): 
    CLAVIPlaylistStreamReader(dev),
    m_chapter(-1),
    m_dvdReader(0),
    m_tmpBufferSize(0)
{
    m_tmpBuffer = new quint8[IO_BLOCK_SIZE * 2];
    m_dvdReadBuffer = new quint8[IO_BLOCK_SIZE + DVD_VIDEO_LB_LEN];
    m_dvdAlignedBuffer = (quint8*) DVD_ALIGN(m_dvdReadBuffer);
}

CLAVIDvdStreamReader::~CLAVIDvdStreamReader()
{
    delete [] m_tmpBuffer;
    delete [] m_dvdReadBuffer;
}

void CLAVIDvdStreamReader::setChapterNum(int chapter)
{
    m_chapter = chapter;
}

QStringList CLAVIDvdStreamReader::getPlaylist()
{
    QStringList rez;
    QString sourceDir = m_device->getUniqueId();
    if (!sourceDir.toUpper().endsWith("VIDEO_TS"))
        sourceDir = CLAVIPlaylistStreamReader::addDirPath(sourceDir, "VIDEO_TS");
    QDir dvdDir = QDir(sourceDir);
    if (!dvdDir.exists())
        return rez;

    QString mask = "VTS_";
    dvdDir.setNameFilters(QStringList() << mask+"*.vob" << mask+"*.VOB");
    dvdDir.setSorting(QDir::Name);
    QFileInfoList tmpFileList = dvdDir.entryInfoList();
    QString chapterStr = QString::number(m_chapter);
    if (m_chapter < 10)
        chapterStr.insert(0, "0");

    for (int i = 0; i < tmpFileList.size(); ++i)
    {
        QStringList vobName = tmpFileList[i].baseName().split("_");
        if ((vobName.size()==3) && (vobName.at(2).left(1) == "1"))
        {
            QFile f(tmpFileList[i].absoluteFilePath());
            f.open(QFile::ReadOnly);
            rez << tmpFileList[i].absoluteFilePath();
        }
    }
    return rez;
}

// ------------------------ encrypted DVD IO context reader ---------------------------

struct CLAVIDvdStreamReaderPriv
{
    static qint32 readPacket(void *opaque, quint8* buf, int size)
    {
        CLAVIDvdStreamReader* reader = reinterpret_cast<CLAVIDvdStreamReader*> (opaque);
        return reader->readPacket(buf, size);
    }

    static qint64 seek(void* opaque, qint64 offset, qint32 whence)
    {
        CLAVIDvdStreamReader* reader = reinterpret_cast<CLAVIDvdStreamReader*> (opaque);
        return reader->seek(offset, whence);
    }

    static qint32 writePacket(void* opaque, quint8* buf, int bufSize)
    {
        CLAVIDvdStreamReader* reader = reinterpret_cast<CLAVIDvdStreamReader*> (opaque);
        return reader->writePacket(buf, bufSize);
    }
};

ByteIOContext* CLAVIDvdStreamReader::getIOContext() 
{
    //QMutexLocker global_ffmpeg_locker(&global_ffmpeg_mutex);
    if (m_ffmpegIOContext == 0)
    {
        m_ffmpegIOContext = av_alloc_put_byte(      
            m_ioBuffer,
            IO_BLOCK_SIZE,
            0,
            this,
            &CLAVIDvdStreamReaderPriv::readPacket,
            &CLAVIDvdStreamReaderPriv::writePacket,
            &CLAVIDvdStreamReaderPriv::seek);
    }
    return m_ffmpegIOContext;
}

bool CLAVIDvdStreamReader::switchToFile(int newFileIndex)
{
    QString fileName = m_fileList[newFileIndex]->m_name;
    if (!m_dvdReader)
    {
        m_dvdReader = DVDOpen(m_device->getUniqueId().toAscii().constData());
        if (!m_dvdReader)
            return false;
    }
    if (newFileIndex >= m_fileList.size())
        return false;

    m_currentPosition = 0;
    m_tmpBufferSize = 0;
    if (newFileIndex != m_currentFileIndex)
    {
        DvdDecryptInfo* data;
        if (m_fileList[newFileIndex]->opaque == 0)
        {
            data = new DvdDecryptInfo();
            m_fileList[newFileIndex]->opaque = data;

            QFileInfo info(m_fileList[newFileIndex]->m_name);
            int number = 1;
            QStringList parts = info.baseName().split('_');
            if (parts.size() > 1)
                number = parts[1].toInt();

            ifo_handle_t* ifo = ifoOpen(m_dvdReader, number);
            if (ifo)
            {
                if (ifo->vts_tmapt && ifo->vts_tmapt->nr_of_tmaps)
                    m_fileList[newFileIndex]->m_durationHint = ifo->vts_tmapt->tmap->nr_of_entries * ifo->vts_tmapt->tmap->tmu * 1000000ull;
                ifoClose(ifo);
            }
            if (m_fileList[newFileIndex]->m_durationHint == 0)
                return false; // skip file

            data->m_dvd_file = DVDOpenFile(m_dvdReader, number, DVD_READ_TITLE_VOBS);
            if (data->m_dvd_file == 0)
                return false;

        }
        m_currentFileIndex = newFileIndex;
        if (m_fileList[newFileIndex]->m_formatContext)
        {
            QMutexLocker mutex(&m_cs);
            m_formatContext = m_fileList[m_currentFileIndex]->m_formatContext;
            initCodecs();
        }
    }
    return true;
}

qint32 CLAVIDvdStreamReader::readPacket(quint8* buf, int size)
{
    size = qMin(size, IO_BLOCK_SIZE);

    CLFileInfo* fi = m_fileList[m_currentFileIndex];
    DvdDecryptInfo* data = (DvdDecryptInfo*) fi->opaque;
    if (data == 0)
        return -1;
    qint64 currentFileSize = DvdGetFileSize(data->m_dvd_file);

    if (m_currentPosition >= currentFileSize && m_currentFileIndex < m_fileList.size()-1)
    {
        switchToFile(m_currentFileIndex+1);
        if (m_fileList[m_currentFileIndex]->m_formatContext)
        {
            QMutexLocker mutex(&m_cs);
            m_formatContext = m_fileList[m_currentFileIndex]->m_formatContext;
            initCodecs();
        }
    }

    int currentBlock = m_currentPosition / DVDCSS_BLOCK_SIZE;
    int blocksToRead = qMin(IO_BLOCK_SIZE/DVDCSS_BLOCK_SIZE, int(currentFileSize/DVDCSS_BLOCK_SIZE - currentBlock));
    while (m_tmpBufferSize < size && blocksToRead > 0)
    {
        int readed = DVDReadBlocks(data->m_dvd_file, currentBlock, blocksToRead, m_dvdAlignedBuffer);
        if (readed >= 0)
        {
            memcpy(m_tmpBuffer + m_tmpBufferSize, m_dvdAlignedBuffer, readed * DVDCSS_BLOCK_SIZE);
            m_tmpBufferSize += readed * DVDCSS_BLOCK_SIZE;
            int offsetRest = m_currentPosition % DVDCSS_BLOCK_SIZE;
            
            if (offsetRest)
            {
                memmove(m_tmpBuffer, m_tmpBuffer + offsetRest, m_tmpBufferSize - offsetRest);
                m_currentPosition -= offsetRest;
                m_tmpBufferSize -= offsetRest;
            }
            
            m_currentPosition += blocksToRead * DVDCSS_BLOCK_SIZE; // increase position always. It is not a bag! So, skip non readable sectors.
            currentBlock += blocksToRead;
        }
        else {
            m_currentPosition += IO_SKIP_BLOCK_SIZE; // skip non readable data
            currentBlock += IO_SKIP_BLOCK_SIZE/DVDCSS_BLOCK_SIZE;
        }
        blocksToRead = qMin(IO_BLOCK_SIZE/DVDCSS_BLOCK_SIZE, int(currentFileSize/DVDCSS_BLOCK_SIZE - currentBlock));
    }

    int availBytes = qMin(size, m_tmpBufferSize);
    memcpy(buf, m_tmpBuffer, availBytes);
    memmove(m_tmpBuffer, m_tmpBuffer + availBytes, m_tmpBufferSize - availBytes);
    m_tmpBufferSize -= availBytes;
    return availBytes;
}

qint64 CLAVIDvdStreamReader::seek(qint64 offset, qint32 whence)
{
    if (m_currentFileIndex == -1)
        return -1;

    CLFileInfo* fi = m_fileList[m_currentFileIndex];
    DvdDecryptInfo* data = (DvdDecryptInfo*) fi->opaque;
    if (data == 0)
        return -1;
    m_tmpBufferSize = 0;
    if (whence == AVSEEK_SIZE)
        return DvdGetFileSize(data->m_dvd_file);

    if (whence == SEEK_SET)
        ; // offsetInFile = offset; // SEEK_SET by default
    else if (whence == SEEK_CUR)
        offset += m_currentPosition;
    else if (whence == SEEK_END)
        offset = DvdGetFileSize(data->m_dvd_file) - offset;
    if (offset < 0)
        return -1; // seek failed
    else if (offset >= DvdGetFileSize(data->m_dvd_file))
        return -1; // seek failed
    
    m_currentPosition = offset;
    return offset;
    
}


qint32 CLAVIDvdStreamReader::writePacket(quint8* buf, int size)
{
    return 0; // not implemented
}


void CLAVIDvdStreamReader::destroy()
{

    foreach(CLFileInfo* fi, m_fileList)
    {
        DvdDecryptInfo* info = (DvdDecryptInfo*) fi->opaque;
        if (info && info->m_dvd_file)
            DVDCloseFile(info->m_dvd_file);
        delete info;
    }
    DVDClose(m_dvdReader);
    m_dvdReader = 0;
    m_tmpBufferSize = 0;
    CLAVIPlaylistStreamReader::destroy();
}
