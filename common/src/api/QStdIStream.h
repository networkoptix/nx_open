#include <QIODevice>
#include <ios>

class QStdStreamBuf : public std::streambuf
{
public:
    QStdStreamBuf(QIODevice *dev);

protected:
    virtual std::streamsize xsgetn(std::streambuf::char_type *str, std::streamsize n);
    virtual std::streamsize xsputn(const std::streambuf::char_type *str, std::streamsize n);
    virtual std::streambuf::pos_type seekoff(std::streambuf::off_type off, std::ios_base::seekdir dir, std::ios_base::openmode /*__mode*/);
    virtual std::streambuf::pos_type seekpos(std::streambuf::pos_type off, std::ios_base::openmode /*__mode*/);
    virtual std::streambuf::int_type underflow();

private:
    static const std::streamsize BUFFER_SIZE = 1024;
    std::streambuf::char_type m_inbuf[BUFFER_SIZE];
    QIODevice *m_dev;
};

class QStdIStream : public std::istream
{
public:
    QStdIStream(QIODevice *dev);

    virtual ~QStdIStream();

private:
    QStdStreamBuf * m_buf;
};


