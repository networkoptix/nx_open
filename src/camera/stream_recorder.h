#ifndef stream_recorder_h_15_14h
#define stream_recorder_h_15_14h

#include "data\dataprocessor.h"
#include "device\device_video_layout.h"
class CLDevice;
class CLByteArray;

class CLStreamRecorder : public CLAbstractDataProcessor
{
public:
	CLStreamRecorder(CLDevice* dev);
	~CLStreamRecorder();

protected:
	virtual void processData(CLAbstractData* data);
	virtual void endOfRun();
	void onFirstdata();
	void cleanup();


	QString filname_data(int channel);
	QString filname_descr(int channel);
	QString filname_helper(int channel);
	QString dir_helper();

	void flush_channel(int channel);
	bool need_to_flush(int channel);
	

protected:
	CLDevice* mDevice;

	CLByteArray* mData[CL_MAX_CHANNELS];
	CLByteArray* mDescr[CL_MAX_CHANNELS];

	bool mFirsTime;
	bool mGotKeyFrame[CL_MAX_CHANNELS];

	FILE* mDataFile[CL_MAX_CHANNELS];
	FILE* mDescrFile[CL_MAX_CHANNELS];

	unsigned int mVersion;

};

#endif //stream_recorder_h_15_14h