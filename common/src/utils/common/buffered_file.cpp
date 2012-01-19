#include "buffered_file.h"

static const int SECTOR_SIZE = 32768;


QBufferedFile::QBufferedFile(const QString& fileName, int bufferSize): 
    QnFile(fileName), 
    m_bufferSize(bufferSize) 
{
    m_buffer = (quint8*) qMallocAligned(bufferSize, SECTOR_SIZE);
    m_bufferLen = 0;
    m_bufferPos = 0;
    m_totalWrited = 0;
    m_isDirectIO = false;
}
QBufferedFile::~QBufferedFile()
{
    qFreeAligned(m_buffer);
}
qint64	QBufferedFile::size () const
{
    if (isWritable())
        return m_totalWrited + m_bufferLen;
    else
        return QnFile::size(); 
}

void QBufferedFile::disableDirectIO()
{
#ifdef Q_OS_WIN
    if (m_isDirectIO)
    {
        QnFile::close();
        QnFile::open(m_openMode, 0);
        QnFile::seek(m_filePos);
    }
#endif
    m_isDirectIO = false;
}

void QBufferedFile::flushBuffer()
{
    disableDirectIO();
    qint64 writed = QnFile::write((char*) m_buffer, m_bufferLen);
    if (writed > 0)
    {
        m_totalWrited += writed;
        m_filePos += writed;
    }
    m_bufferLen = 0;
    m_bufferPos = 0;
}

void QBufferedFile::close()
{
    flushBuffer();
    QnFile::close();
}

qint64	QBufferedFile::read (char * data, qint64 len )
{
    qint64 rez = QnFile::read(data, len);
    if (rez > 0)
        m_filePos += rez;
    return rez;
}

qint64	QBufferedFile::write ( const char * data, qint64 len )
{
    int rez = len;
    while (len > 0)
    {
        int toWrite = qMin((int) len, m_bufferSize - m_bufferPos);
        memcpy(m_buffer + m_bufferPos, data, toWrite);
        m_bufferPos += toWrite;
        m_bufferLen = qMax(m_bufferLen, m_bufferPos);
        if (m_bufferLen == m_bufferSize) {
            int writed = QnFile::write((char*) m_buffer, m_bufferSize/2);
            m_totalWrited += writed;
            if (writed !=  m_bufferSize/2)
                return writed;
            m_filePos += m_bufferLen/2;
            m_bufferLen -= writed;
            m_bufferPos -= writed;
            memmove(m_buffer, m_buffer + writed, m_bufferLen);
        }
        len -= toWrite;
        data += toWrite;
    }
    return rez;
}

bool QBufferedFile::seek(qint64 pos)
{
    qint64 bufferOffset = pos - (size() - m_bufferLen);
    if (bufferOffset < 0 || bufferOffset > m_bufferLen)
    {
        flushBuffer();
        m_filePos = pos;
        return QnFile::seek(pos);
    }
    else {
        m_bufferPos = bufferOffset;
        return true;
    }
}

bool QBufferedFile::open(QIODevice::OpenMode& mode, unsigned int systemDependentFlags)
{
    m_openMode = mode;
    QDir dir;
    dir.mkpath(QFileInfo(m_fileName).absoluteDir().absolutePath());
    m_isDirectIO = false;
#ifdef Q_OS_WIN
    m_isDirectIO = systemDependentFlags & FILE_FLAG_NO_BUFFERING;
#endif
    bool rez = QnFile::open(mode, systemDependentFlags);
    m_filePos = 0;
    return rez;
}

bool QBufferedFile::isWritable() const
{
    return m_openMode & QIODevice::WriteOnly;
}

qint64	QBufferedFile::pos() const
{
    return m_filePos + m_bufferPos;
}
