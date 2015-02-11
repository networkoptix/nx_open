#include "ringbuffer.h"

CLRingBuffer::CLRingBuffer( unsigned int capacity, QObject* parent)
    : QIODevice(parent),
      m_capacity(capacity),
      m_mtx(QnMutex::Recursive)
{
	m_buff = new char[m_capacity];
	m_pw = m_buff;
	m_pr = m_buff;

	open(QIODevice::ReadWrite);
}

CLRingBuffer::~CLRingBuffer()
{
	delete[] m_buff;
}

void CLRingBuffer::clear()
{
	m_pr = m_buff;
	m_pw = m_buff;
}

qint64 CLRingBuffer::bytesAvailable() const
{
	SCOPED_MUTEX_LOCK( mutex, &m_mtx);

	return (m_pr <= m_pw) ? m_pw - m_pr : m_capacity - (m_pr - m_pw);
}

qint64 CLRingBuffer::avalable_to_write() const
{
	SCOPED_MUTEX_LOCK( mutex, &m_mtx);

	return (m_pw >= m_pr) ? m_capacity - (m_pw - m_pr) : m_pr - m_pw;
}

qint64 CLRingBuffer::readData(char *data, qint64 maxlen)
{
	SCOPED_MUTEX_LOCK( mutex, &m_mtx);

	qint64 canRead = bytesAvailable();
	qint64 toRead = qMin(canRead, maxlen);

	if (m_pr + toRead <= m_buff + m_capacity)
	{
		memcpy(data, m_pr, toRead);
		m_pr += toRead;
	}
	else
	{
		int first_read = m_buff + m_capacity - m_pr;
		memcpy(data, m_pr, first_read);

		int second_read = toRead - first_read;
		memcpy(data + first_read, m_buff, second_read);
		m_pr = m_buff + second_read;
	}

	return toRead;
}

qint64 CLRingBuffer::writeData(const char *data, qint64 len)
{
	SCOPED_MUTEX_LOCK( mutex, &m_mtx);

	qint64 can_write = avalable_to_write();
	qint64 to_write = qMin(len, can_write);

	if (m_pw + to_write <= m_buff + m_capacity)
	{
		memcpy(m_pw, data, to_write);
		m_pw += to_write;
	}
	else
	{
		int first_write = m_buff + m_capacity - m_pw;
		memcpy(m_pw, data, first_write);

		int second_write = to_write - first_write;
		memcpy(m_buff, data + first_write, second_write);
		m_pw = m_buff + second_write;
	}

	return to_write;
}

qint64 CLRingBuffer::readToIODevice(QIODevice* device, qint64 maxlen)
{
    //maxlen = qMin(maxlen, divce->bytesToWrite());

    SCOPED_MUTEX_LOCK( mutex, &m_mtx);

    qint64 canRead = bytesAvailable();
    qint64 toRead = qMin(canRead, maxlen);

    if (m_pr + toRead <= m_buff + m_capacity)
    {
        device->write(m_pr, toRead);
        m_pr += toRead;
    }
    else
    {
        int firstRead = m_buff + m_capacity - m_pr;
        device->write(m_pr, firstRead);

        int secondRead = toRead - firstRead;
        device->write(m_buff, secondRead);
        m_pr = m_buff + secondRead;
    }

    return toRead;
}

unsigned int CLRingBuffer::capacity() const
{
    return m_capacity;
}
