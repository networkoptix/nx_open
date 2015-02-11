#ifndef CL_ringbuffer_1842
#define CL_ringbuffer_1842

#include <QtCore/QIODevice>
#include <utils/common/mutex.h>


class CLRingBuffer : public QIODevice
{
public:
	CLRingBuffer(unsigned int capacity, QObject* parent = 0);
	~CLRingBuffer();

	qint64 readData(char *data, qint64 maxlen);
	qint64 writeData(const char *data, qint64 len);

    /**
      * Reads data and puts it to IOdevice
      */
    qint64 readToIODevice(QIODevice* device, qint64 maxlen);
	qint64 bytesAvailable() const;
	qint64 avalable_to_write() const;
	void clear();
    unsigned int capacity() const;

private:
	char *m_buff;
	unsigned int m_capacity;

	char* m_pw;
	char* m_pr;

	mutable QnMutex m_mtx;
};

#endif //CL_ringbuffer_1842
