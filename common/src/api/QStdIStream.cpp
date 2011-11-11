#include "QStdIStream.h"

QStdStreamBuf::QStdStreamBuf(QIODevice *dev) : std::streambuf(), m_dev(dev)
{
    // Initialize get pointer.  This should be zero so that underflow is called upon first read.
    this->setg(0, 0, 0);
}

std::streamsize QStdStreamBuf::xsgetn(std::streambuf::char_type *str, std::streamsize n)
{
    return m_dev->read(str, n);
}

std::streamsize QStdStreamBuf::xsputn(const std::streambuf::char_type *str, std::streamsize n)
{
    return m_dev->write(str, n);
}

std::streambuf::pos_type QStdStreamBuf::seekoff(std::streambuf::off_type off, std::ios_base::seekdir dir, std::ios_base::openmode /*__mode*/)
{
    switch(dir)
    {
        case std::ios_base::beg:
            break;
        case std::ios_base::end:
            off = m_dev->size() - off;
            break;
        case std::ios_base::cur:
            off = m_dev->pos() + off;
            break;
    }

    if(m_dev->seek(off))
        return m_dev->pos();
    else
        return std::streambuf::pos_type(std::streambuf::off_type(-1));
}

std::streambuf::pos_type QStdStreamBuf::seekpos(std::streambuf::pos_type off, std::ios_base::openmode /*__mode*/)
{
    if(m_dev->seek(off))
        return m_dev->pos();
    else
        return std::streambuf::pos_type(std::streambuf::off_type(-1));
}

std::streambuf::int_type QStdStreamBuf::underflow()
{
    // Read enough bytes to fill the buffer.
    std::streamsize len = sgetn(m_inbuf, sizeof(m_inbuf)/sizeof(m_inbuf[0]));

    // Since the input buffer content is now valid (or is new)
    // the get pointer should be initialized (or reset).
    setg(m_inbuf, m_inbuf, m_inbuf + len);

    // If nothing was read, then the end is here.
    if(len == 0)
        return traits_type::eof();

    // Return the first character.
    return traits_type::not_eof(m_inbuf[0]);
}

QStdIStream::QStdIStream(QIODevice *dev)
    : std::istream(m_buf = new QStdStreamBuf(dev)) 
{}

QStdIStream::~QStdIStream()
{
    rdbuf(0);
    delete m_buf;
}

