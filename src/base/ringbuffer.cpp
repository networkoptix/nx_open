#include "ringbuffer.h"


CLRingBuffer::CLRingBuffer( int capacity, QObject* parent):
QIODevice(parent),
m_capacity(capacity),
m_mtx(QMutex::Recursive)
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
	QMutexLocker mutex(&m_mtx);
	return (m_pr <= m_pw) ? m_pw - m_pr : m_capacity - (m_pr - m_pw);
}

qint64 CLRingBuffer::avalable_to_write() const
{
	QMutexLocker mutex(&m_mtx);
	return (m_pw >= m_pr) ? m_capacity - (m_pw - m_pr) : m_pr - m_pw;
}


qint64 CLRingBuffer::readData(char *data, qint64 maxlen)
{
	QMutexLocker mutex(&m_mtx);

	qint64 can_read = bytesAvailable();

	qint64 to_read = qMin(can_read, maxlen);


	if (m_pr + to_read<= m_buff + m_capacity)
	{
		memcpy(data, m_pr, to_read);
		m_pr+=to_read;

	}
	else
	{
		int first_read = m_buff + m_capacity - m_pr;
		memcpy(data, m_pr, first_read);

		int second_read = to_read - first_read;
		memcpy(data + first_read, m_buff, second_read);
		m_pr = m_buff + second_read;


	}

	return to_read;


}

qint64 CLRingBuffer::writeData(const char *data, qint64 len)
{
	QMutexLocker mutex(&m_mtx);

	qint64 can_write = avalable_to_write();


	qint64 to_write = qMin(len, can_write);

	if (m_pw + to_write <= m_buff + m_capacity)
	{
		memcpy(m_pw, data, to_write);
		m_pw+=to_write;

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

