#include "avi_playlist_archive_delegate.h"

#include <QtCore/QMutex>

#include <core/resource/resource.h>
#include "utils/common/util.h"

/*
extern "C"
{
    // this function placed at <libavformat/internal.h> header
    void ff_read_frame_flush(AVFormatContext *s);
};
*/

static void free_packet_buffer(AVPacketList **pkt_buf, AVPacketList **pkt_buf_end)
{
    while (*pkt_buf) {
        AVPacketList *pktl = *pkt_buf;
        *pkt_buf = pktl->next;
        av_free_packet(&pktl->pkt);
        av_freep(&pktl);
    }
    *pkt_buf_end = NULL;
}

static void flush_packet_queue(AVFormatContext *s)
{
    free_packet_buffer(&s->parse_queue,       &s->parse_queue_end);
    free_packet_buffer(&s->packet_buffer,     &s->packet_buffer_end);
    free_packet_buffer(&s->raw_packet_buffer, &s->raw_packet_buffer_end);

    s->raw_packet_buffer_remaining_size = RAW_PACKET_BUFFER_SIZE;
}

void ff_read_frame_flush(AVFormatContext *s)
{
#define RELATIVE_TS_BASE (INT64_MAX - (1LL<<48))

    AVStream *st;
    flush_packet_queue(s);

    /* for each stream, reset read state */
    for(uint i = 0; i < s->nb_streams; i++) {
        st = s->streams[i];

        //if (st->parser) {
        //    av_parser_close(st->parser);
        //    st->parser = NULL;
        // }
        st->last_IP_pts = AV_NOPTS_VALUE;
        if(st->first_dts == qint64(AV_NOPTS_VALUE)) st->cur_dts = RELATIVE_TS_BASE;
        else                                st->cur_dts = AV_NOPTS_VALUE; /* we set the current DTS to an unspecified origin */
        st->reference_dts = AV_NOPTS_VALUE;

        st->probe_packets = MAX_PROBE_PACKETS;

        for(uint j=0; j<MAX_REORDER_DELAY+1; j++)
            st->pts_buffer[j]= AV_NOPTS_VALUE;
    }
}


static const int IO_BLOCK_SIZE = 1024 * 32;

QnAVIPlaylistArchiveDelegate::QnAVIPlaylistArchiveDelegate() :
    QnAviArchiveDelegate(),
    m_ioBuffer(0),
    m_ffmpegIOContext(0),
    m_currentFileIndex(-1),
    m_inSeek(false),
    m_totalContentLength(0),
    m_defaultVideoLayout( new QnDefaultResourceVideoLayout() )
{
}

QnAVIPlaylistArchiveDelegate::~QnAVIPlaylistArchiveDelegate()
{
    close();
    if (m_ffmpegIOContext)
    {
        av_free(m_ffmpegIOContext);
        m_ffmpegIOContext = 0;
    }
    if (m_ioBuffer)
        av_free(m_ioBuffer);
}

bool QnAVIPlaylistArchiveDelegate::open(const QnResourcePtr& resource)
{
    m_resource = resource;
    return true;
}

bool QnAVIPlaylistArchiveDelegate::findStreams()
{
    m_totalContentLength = 0;
    if (m_initialized)
    {
        if (m_fileList.isEmpty())
            return false;
        else {
            m_formatContext = m_fileList[0]->m_formatContext;
            return true;
        }
    }

    QStringList tmpFileList = getPlaylist(m_resource->getUrl());
    m_fileList.clear();

    for (int i = 0; i < tmpFileList.size(); ++i)
    {
        CLFileInfo* fi = new CLFileInfo();
        fi->m_name = tmpFileList[i];
        fi->m_offsetInMks = 0;
        fi->opaque = 0;
        fi->m_formatContext = 0;

        m_fileList << fi;

        if (!switchToFile(m_fileList.size() - 1))
        {
            deleteFileInfo(fi);
            m_fileList.remove(m_fileList.size() - 1);
            continue;
        }
        AVProbeData probeData;
        int bufferSize = 384*1024; // + 2048;
        unsigned char * buffer = new unsigned char[bufferSize];
        //char* alignedBuffer = (char*) (long(buffer + 2047) & ~0x7f);
        probeData.filename = "";
        probeData.buf = buffer;
        probeData.buf_size = 0;
        while (probeData.buf_size < bufferSize)
        {
            int readed = readPacket( buffer + probeData.buf_size, bufferSize - probeData.buf_size);
            if (readed > 0)
                probeData.buf_size += readed;
            else
                break;
        }

        AVInputFormat* inCtx = av_probe_input_format(&probeData, 1);
        delete [] buffer;
        if (!switchToFile(m_fileList.size() - 1))
        {
            deleteFileInfo(fi);
            m_fileList.remove(m_fileList.size() - 1);
            continue;
        }

        //if (!inCtx || av_open_input_stream(&fi->m_formatContext, getIOContext(), "", inCtx, 0) < 0 ||
        //    av_find_stream_info(fi->m_formatContext) < 0)

        fi->m_formatContext->pb = getIOContext();
        if (!inCtx || avformat_open_input(&fi->m_formatContext, "", 0, 0) < 0 ||
            avformat_find_stream_info(fi->m_formatContext, NULL) < 0)
        {
            deleteFileInfo(fi);
            m_fileList.remove(m_fileList.size() - 1);
        }
        else
        {
            fillAdditionalInfo(fi);
            m_totalContentLength += fi->m_formatContext->duration;
            if (m_fileList.size() > 1)
                fi->m_offsetInMks = m_fileList[m_fileList.size()-2]->m_offsetInMks + m_fileList[m_fileList.size()-2]->m_formatContext->duration;
        }

    }
    m_currentFileIndex = -1;
    m_initialized = true;
    if (m_fileList.isEmpty())
        return false;
    else
    {
        switchToFile(0);
        m_formatContext = m_fileList[0]->m_formatContext;
        initLayoutStreams();
        return true;
    }
}

qint64 QnAVIPlaylistArchiveDelegate::findFileIndexByTime(quint64 mksec)
{
    int newFileIndex = 0;
    foreach(CLFileInfo* fi, m_fileList)
    {
       if (mksec < (quint64)fi->m_formatContext->duration)
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
}

qint64 QnAVIPlaylistArchiveDelegate::seek(qint64 mksec, bool /*findIFrame*/)
{
    int oldFileNum = m_currentFileIndex;
    Q_UNUSED(oldFileNum);
    quint64 relativeMksec = findFileIndexByTime(mksec);
    if (relativeMksec == (quint64)-1)
        return -1; // error seeking

    m_startMksec = m_fileList[m_currentFileIndex]->m_formatContext->start_time;
    if ((quint64)m_startMksec != AV_NOPTS_VALUE)
        relativeMksec += m_startMksec;
    m_inSeek = true;
    if (directSeekToPosition(relativeMksec))
    {
        ff_read_frame_flush(m_formatContext);
    } else
    {
        avformat_seek_file(m_formatContext, -1, 0, relativeMksec, LLONG_MAX, AVSEEK_FLAG_BACKWARD);
    }

    m_inSeek = false;
    return mksec;
}

qint64 QnAVIPlaylistArchiveDelegate::packetTimestamp(const AVPacket& packet)
{
    AVStream* stream = m_fileList[m_currentFileIndex]->m_formatContext->streams[packet.stream_index];
    static AVRational r = {1, 1000000};
    qint64 packetTime = av_rescale_q(packet.dts, stream->time_base, r);
    quint64 st = m_fileList[m_currentFileIndex]->m_formatContext->start_time;
    if (st != AV_NOPTS_VALUE)
        return packetTime - st + m_fileList[m_currentFileIndex]->m_offsetInMks;
    else
        return packetTime + m_fileList[m_currentFileIndex]->m_offsetInMks;
}

void QnAVIPlaylistArchiveDelegate::deleteFileInfo(CLFileInfo* fi)
{
    if (fi->m_formatContext)
        avformat_close_input(&fi->m_formatContext);
    delete fi;
}

void QnAVIPlaylistArchiveDelegate::close()
{
    foreach(CLFileInfo* fi, m_fileList)
    {
        deleteFileInfo(fi);
    }
    m_formatContext = 0;
    m_fileList.clear();

    m_initialized = false;
    m_currentFileIndex = -1;
}

QString QnAVIPlaylistArchiveDelegate::addDirPath(const QString sourceDir, const QString& postfix)
{
    QString rez = sourceDir;
    if (!sourceDir.endsWith(QLatin1Char('/')) && !sourceDir.endsWith(QLatin1Char('\\')))
        rez += QLatin1Char('/'); //QDir::separator();
    rez += postfix;
    return rez;
}

// --------------  standart IO context from a file. ---------------------

struct CLAVIPlaylistStreamReaderPriv
{
    static qint32 readPacket(void *opaque, quint8* buf, int size)
    {
        QnAVIPlaylistArchiveDelegate* reader = reinterpret_cast<QnAVIPlaylistArchiveDelegate*> (opaque);
        return reader->readPacket(buf, size);
    }

    static int64_t seek(void* opaque, int64_t offset, qint32 whence)
    {
        QnAVIPlaylistArchiveDelegate* reader = reinterpret_cast<QnAVIPlaylistArchiveDelegate*> (opaque);
        return reader->seek(offset, whence);
    }

    static qint32 writePacket(void* opaque, quint8* buf, int bufSize)
    {
        QnAVIPlaylistArchiveDelegate* reader = reinterpret_cast<QnAVIPlaylistArchiveDelegate*> (opaque);
        return reader->writePacket(buf, bufSize);
    }
};

AVIOContext* QnAVIPlaylistArchiveDelegate::getIOContext()
{
    if (m_ffmpegIOContext == 0)
    {
        m_ioBuffer = (quint8*) av_malloc(IO_BLOCK_SIZE);
        m_ffmpegIOContext = avio_alloc_context(
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

qint64 QnAVIPlaylistArchiveDelegate::seek(qint64 offset, qint32 whence)
{
    if (m_currentFileIndex == -1)
        return -1;
    if (whence == AVSEEK_SIZE)
        return m_currentFile.size();

    if (whence == SEEK_SET)
        ; // offsetInFile = offset; // SEEK_SET by default
    else if (whence == SEEK_CUR)
        offset += m_currentFile.pos();
    else if (whence == SEEK_END)
        offset = m_currentFile.size() - offset;
    if (offset < 0)
        return -1; // seek failed
    else if (offset >= m_currentFile.size())
        return -1; // seek failed

    if (!m_currentFile.seek(offset))
        return -1;

    return offset;
}

bool QnAVIPlaylistArchiveDelegate::switchToFile(int newFileIndex)
{
    if (newFileIndex != m_currentFileIndex && newFileIndex < m_fileList.size())
    {
        m_currentFile.close();
        m_currentFile.setFileName(m_fileList[newFileIndex]->m_name);
        if (!m_currentFile.open(QFile::ReadOnly))
            return false;
        m_currentFileIndex = newFileIndex;
        if (m_fileList[newFileIndex]->m_formatContext)
        {
            m_formatContext = m_fileList[m_currentFileIndex]->m_formatContext;
            setAudioChannel(m_selectedAudioChannel);
        }
    }
    else
    {
        m_currentFile.seek(0);
    }
    return true;
}

qint32 QnAVIPlaylistArchiveDelegate::readPacket(quint8* buf, int size)
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
            if (m_fileList[m_currentFileIndex]->m_formatContext)
            {
                m_formatContext = m_fileList[m_currentFileIndex]->m_formatContext;
                setAudioChannel(m_selectedAudioChannel);
            }
        }
        rez = m_currentFile.read( (char*) buf, size);
    }
    return rez;
}

qint32 QnAVIPlaylistArchiveDelegate::writePacket(quint8* /*buf*/, int /*size*/)
{
    return 0; // not implemented
}

QnResourceVideoLayoutPtr QnAVIPlaylistArchiveDelegate::getVideoLayout()
{
    return m_defaultVideoLayout;
}

qint64 QnAVIPlaylistArchiveDelegate::endTime() 
{
    if (!m_initialized && !findStreams())
        return 0;        
    return m_totalContentLength; 
}
