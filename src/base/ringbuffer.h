#ifndef CL_ringbuffer_1842
#define CL_ringbuffer_1842


class CLRingBuffer : public QIODevice
{

public:
	CLRingBuffer( int capacity, QObject* parent);
	~CLRingBuffer();

	qint64 readData(char *data, qint64 maxlen);
	qint64 writeData(const char *data, qint64 len);
	qint64 bytesAvailable() const;

	qint64 avalable_to_write() const;

	void claer();

private:
	char *m_buff;
	int m_capacity;

	char* m_pw;
	char* m_pr;

	mutable QMutex m_mtx;

};


#endif //CL_ringbuffer_1842