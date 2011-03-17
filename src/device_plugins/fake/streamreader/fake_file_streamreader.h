#ifndef FakeStreamReader_h_106
#define FakeStreamReader_h_106

#include "../../../streamreader/cpull_stremreader.h"
#include "../../../base/adaptivesleep.h"
#include "../../../device/device_video_layout.h"

class FakeStreamReader : public CLClientPullStreamreader
{
public:
	explicit FakeStreamReader(CLDevice* dev, int channels);

	~FakeStreamReader();

protected:
	virtual CLAbstractMediaData* getNextData();

protected:

private:

	int m_channels;
	int m_curr_channel;
	quint64 m_time;

	static unsigned char* data;
	static unsigned char* descr;
	unsigned char* pdata[CL_MAX_CHANNELS];
	int* pdescr[CL_MAX_CHANNELS];

	static int data_len;
	static int descr_data_len;

	CLAdaptiveSleep m_sleep;

};

#endif //FakeStreamReader_h_106