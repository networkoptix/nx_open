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
	void onFirstData(CLAbstractData* data);
	void cleanup();

	QString filenameData(int channel);
	QString filenameDescription(int channel);
	QString filenameHelper(int channel);
	QString dirHelper();

	void flushChannel(int channel);
	bool needToFlush(int channel);

protected:
	CLDevice* m_device;

	CLByteArray* m_data[CL_MAX_CHANNELS];
	CLByteArray* m_description[CL_MAX_CHANNELS];

	bool m_firstTime;
	bool m_gotKeyFrame[CL_MAX_CHANNELS];

	FILE* m_dataFile[CL_MAX_CHANNELS];
	FILE* m_descriptionFile[CL_MAX_CHANNELS];

	unsigned int m_version;
};

#endif //stream_recorder_h_15_14h