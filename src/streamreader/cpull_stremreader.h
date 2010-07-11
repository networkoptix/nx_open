#ifndef client_pull_stream_reader_h1226
#define client_pull_stream_reader_h1226

#include "streamreader.h"

struct CLAbstractMediaData;

#define CL_MAX_CHANNEL_NUMBER (10) 


class CLClientPullStreamreader : public CLStreamreader
{
public:
	CLClientPullStreamreader(CLDevice* dev );
	~CLClientPullStreamreader(){stop();}

	virtual void needKeyData();
protected:
	virtual CLAbstractMediaData* getNextData() = 0;
	bool needToRead() const;

	bool isKeyDataReceived() const;
private:
	void run(); // in a loop: takes images from camera and put into queue

private:
	bool m_key_farme_helper[CL_MAX_CHANNEL_NUMBER];
	int m_channel_number;

};

#endif //client_pull_stream_reader_h1226