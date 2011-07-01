#include <QMutex>

#include "avi_playlist_strem_reader.h"


extern QMutex global_ffmpeg_mutex;
static const int IO_BLOCK_SIZE = 1024 * 32;

// FFMPEG static functions for IO context

struct CLAVIPlaylistStreamReaderPriv
{
    static qint32 readPacket(void *opaque, quint8* buf, int size)
    {
        CLAVIPlaylistStreamReader* reader = reinterpret_cast<CLAVIPlaylistStreamReader*> (opaque);
        return reader->readPacket(buf, size);
    }

    static qint64 seek(void* opaque, qint64 offset, qint32 whence)
    {
        CLAVIPlaylistStreamReader* reader = reinterpret_cast<CLAVIPlaylistStreamReader*> (opaque);
        return reader->seek(offset, whence);
    }

    static qint32 writePacket(void* opaque, quint8* buf, int bufSize)
    {
        CLAVIPlaylistStreamReader* reader = reinterpret_cast<CLAVIPlaylistStreamReader*> (opaque);
        return reader->writePacket(buf, bufSize);
    }
};

/////////////////////////////////////////////////////////////

CLAVIPlaylistStreamReader::CLAVIPlaylistStreamReader(CLDevice* dev):
    CLAVIStreamReader(dev),
    m_ffmpegIOContext(0),
    m_initialized(false),
    m_currentFileIndex(-1),
    m_inSeek(false)
{
    QMutexLocker global_ffmpeg_locker(&global_ffmpeg_mutex);
    m_ioBuffer = (quint8*) av_malloc(IO_BLOCK_SIZE);
}

CLAVIPlaylistStreamReader::~CLAVIPlaylistStreamReader()
{
    destroy();
    if (m_ffmpegIOContext)
        av_free(m_ffmpegIOContext);
    av_free(m_ioBuffer);
}

ByteIOContext* CLAVIPlaylistStreamReader::getIOContext() 
{
    //QMutexLocker global_ffmpeg_locker(&global_ffmpeg_mutex);
    if (m_ffmpegIOContext == 0)
    {
        m_ffmpegIOContext = av_alloc_put_byte(      
            m_ioBuffer,
            IO_BLOCK_SIZE,
            0,
            this,
            &CLAVIPlaylistStreamReaderPriv::readPacket,
            &CLAVIPlaylistStreamReaderPriv::writePacket,
            &CLAVIPlaylistStreamReaderPriv::seek);
    }
    return m_ffmpegIOContext;
}

AVFormatContext* CLAVIPlaylistStreamReader::getFormatContext()
{
    if (m_initialized)
    {
        if (m_fileList.isEmpty())
            return 0;
        else 
            return m_fileList[0]->m_formatContext;
    }

    QStringList tmpFileList = getPlaylist();
    m_fileList.clear();

    for (int i = 0; i < tmpFileList.size(); ++i)
    {
        CLFileInfo* fi = new CLFileInfo();
        fi->m_name = tmpFileList[i];
        fi->m_offsetInMks = 0;
        QString url = QString("ufile:") + fi->m_name;
        if (av_open_input_file(&fi->m_formatContext, url.toUtf8().constData(), NULL, 0, NULL) >= 0)
        {
            QMutexLocker global_ffmpeg_locker(&global_ffmpeg_mutex);
            if (av_find_stream_info(fi->m_formatContext) >= 0)
            {
                m_lengthMksec += fi->m_formatContext->duration;
                if (!m_fileList.isEmpty())
                    fi->m_offsetInMks = m_fileList.last()->m_offsetInMks + m_fileList.last()->m_formatContext->duration;
            }
            fi->m_orig_pb = fi->m_formatContext->pb;
            fi->m_formatContext->pb = getIOContext();
            m_fileList << fi;
        }
        else {
            delete fi;
        }
    }
    m_currentFileIndex = -1;
    m_initialized = true;
    switchToFile(0);
    if (m_fileList.isEmpty())
        return 0;
    else 
        return m_fileList[0]->m_formatContext;
}

qint64 CLAVIPlaylistStreamReader::seek(qint64 offset, qint32 whence)
{
    if (m_currentFileIndex == -1)
        return -1;
    if (whence == AVSEEK_SIZE)
        return m_fileList[m_currentFileIndex]->m_formatContext->file_size;
    qint64 offsetInFile = m_currentFile.pos();
    qint64 prevFileSize = 0;
    for (int i = 0; i < m_currentFileIndex; ++i)
        prevFileSize += m_fileList[i]->m_formatContext->file_size;
    offsetInFile -= prevFileSize;

    if (whence == SEEK_SET)
        offsetInFile = offset; // SEEK_SET by default
    else if (whence == SEEK_CUR)
        offsetInFile += offset;
    else if (whence == SEEK_END)
        offsetInFile = m_fileList[m_currentFileIndex]->m_formatContext->file_size - offset;
    if (offsetInFile < 0)
        return -1; // seek failed
    else if (offsetInFile >= m_fileList[m_currentFileIndex]->m_formatContext->file_size)
        return -1; // seek failed

    if (!m_currentFile.seek(offsetInFile))
        return -1;

    return offsetInFile;
}

bool CLAVIPlaylistStreamReader::switchToFile(int newFileIndex)
{
    if (newFileIndex != m_currentFileIndex && newFileIndex < m_fileList.size())
    {
        m_currentFile.close();
        m_currentFile.setFileName(m_fileList[newFileIndex]->m_name);
        if (!m_currentFile.open(QFile::ReadOnly))
            return false;
        m_currentFileIndex = newFileIndex;
        QMutexLocker mutex(&m_cs);
        m_formatContext = m_fileList[m_currentFileIndex]->m_formatContext;
        initCodecs();
    }
    return true;
}

qint32 CLAVIPlaylistStreamReader::readPacket(quint8* buf, int size)
{
    int rez = m_currentFile.read( (char*) buf, size);
    if (rez == 0 && m_currentFileIndex < m_fileList.size()-1 && !m_inSeek)
    {
        m_currentFileIndex++;
        m_currentFile.close();
        m_currentFile.setFileName(m_fileList[m_currentFileIndex]->m_name);
        if (!m_currentFile.open(QFile::ReadOnly))
            return 0;
        {
            QMutexLocker mutex(&m_cs);
            m_formatContext = m_fileList[m_currentFileIndex]->m_formatContext;
            initCodecs();
        }
        rez = m_currentFile.read( (char*) buf, size);
    }
    return rez;
}

qint32 CLAVIPlaylistStreamReader::writePacket(quint8* buf, int size)
{
    return 0; // not implemented
}

qint64 CLAVIPlaylistStreamReader::findFileIndexByTime(quint64 mksec)
{
    int newFileIndex = 0;
    foreach(CLFileInfo* fi, m_fileList)
    {
       if (mksec < fi->m_formatContext->duration)
           break;
       mksec -= fi->m_formatContext->duration;
       newFileIndex++;
    }
    if (newFileIndex < m_fileList.size()) 
    {
        switchToFile(newFileIndex); // switch to nesessary file
        return mksec;
    }  
    else {
        return -1;
    }
};

void CLAVIPlaylistStreamReader::channeljumpTo(quint64 mksec, int )
{
    QMutexLocker mutex(&m_cs);
    int oldFileNum = m_currentFileIndex;
    quint64 relativeMksec = findFileIndexByTime(mksec);
    if (relativeMksec == -1)
        return; // error seeking

    m_startMksec = m_fileList[m_currentFileIndex]->m_formatContext->start_time;
    if (m_startMksec != AV_NOPTS_VALUE)
    {
        relativeMksec += m_startMksec;
    }
    int rez;
    m_inSeek = true;
#ifdef _WIN32
    rez = avformat_seek_file(m_formatContext, -1, 0, relativeMksec, _I64_MAX, AVSEEK_FLAG_BACKWARD);
#else
    rez = avformat_seek_file(m_formatContext, -1, 0, relativeMksec, LLONG_MAX, AVSEEK_FLAG_BACKWARD);
#endif
    m_inSeek = false;
    m_needToSleep = 0;
    m_previousTime = -1;
    m_wakeup = true;
}

qint64 CLAVIPlaylistStreamReader::packetTimestamp(AVStream* stream, const AVPacket& packet)
{
    static AVRational r = {1, 1000000};
    qint64 packetTime = av_rescale_q(packet.dts, stream->time_base, r);
    qint64 st = m_fileList[m_currentFileIndex]->m_formatContext->start_time;
    if (st != AV_NOPTS_VALUE)
        return packetTime - st + m_fileList[m_currentFileIndex]->m_offsetInMks;
}

void CLAVIPlaylistStreamReader::destroy()
{
    
    foreach(CLFileInfo* fi, m_fileList)
    {
        fi->m_formatContext->pb = fi->m_orig_pb;
        if (fi->m_formatContext)
            av_close_input_file(fi->m_formatContext);
        delete fi;
    }

    /*
    if (m_ffmpegIOContext)
    {
        av_free(m_ffmpegIOContext);
        m_ffmpegIOContext = 0;
    }
    */

    m_initialized = false;
    m_currentFileIndex = -1;
    m_formatContext = 0;
    m_fileList.clear();
}

QString CLAVIPlaylistStreamReader::addDirPath(const QString sourceDir, const QString& postfix)
{
    QString rez = sourceDir;
    if (!sourceDir.endsWith('/') && !sourceDir.endsWith('\\'))
        rez += '/'; //QDir::separator();
    rez += postfix;
    return rez;
}

