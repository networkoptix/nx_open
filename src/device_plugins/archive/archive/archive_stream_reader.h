#ifndef archive_stream_reader_h1145
#define archive_stream_reader_h1145

#include "..\abstract_archive_stream_reader.h"
#include <QFile>

class CLArchiveStreamReader  : public CLAbstractArchiveReader 
{
public:
	CLArchiveStreamReader(CLDevice* dev);
	~CLArchiveStreamReader();
protected:
	virtual void jumpTo(unsigned long msec, int channel);
	virtual CLAbstractMediaData* getNextData();

	// i do not want to do it in constructor to unload gui thread 
	// let reader thread do the work
	// this function will be called with first getNextData
	void init_data();

	void parse_channel_data(int channel, int data_version, char* data, unsigned int len);
protected:
	bool m_firsttime;
	
	QFile m_data_file[CL_MAX_CHANNELS];
	QDataStream m_data_stream[CL_MAX_CHANNELS];
	

};

#endif //archive_stream_reader_h1145


