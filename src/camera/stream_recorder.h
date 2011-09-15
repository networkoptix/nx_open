#ifndef stream_recorder_h_15_14h
#define stream_recorder_h_15_14h

#include "data/dataprocessor.h"
#include "device/device_video_layout.h"
#include "base/bytearray.h"
class CLDevice;
class CLByteArray;
class CLCompressedVideoData;

class CLStreamRecorder : public CLAbstractDataProcessor
{
public:
	CLStreamRecorder(CLDevice* dev);
	~CLStreamRecorder();

protected:
	virtual bool processData(CLAbstractData* data);
	virtual void endOfRun();
	void onFirstData(CLAbstractData* data);
	void cleanup();

	QString filenameData(int channel);
	QString filenameDescription(int channel);
	QString filenameHelper(int channel);
	QString dirHelper();

    bool initFfmpegContainer(CLCompressedVideoData* mediaData, CLDevice* dev);
    void correctH264Bitstream(AVPacket& packet);
protected:
	CLDevice* m_device;
	CLByteArray* m_data[CL_MAX_CHANNELS];
	CLByteArray* m_description[CL_MAX_CHANNELS];

	bool m_firstTime;
	bool m_gotKeyFrame[CL_MAX_CHANNELS];

	FILE* m_dataFile[CL_MAX_CHANNELS];
	FILE* m_descriptionFile[CL_MAX_CHANNELS];

	unsigned int m_version;

private:
    ByteIOContext* m_iocontext;
    QFile* m_outputFile;
    AVFormatContext* m_formatCtx;
    AVOutputFormat * m_outputCtx;
    QVector<quint8> m_fileBuffer;
    CLByteArray m_tmpPacketBuffer;
    bool m_videoPacketWrited;
    qint64 m_firstTimestamp;
private:
    int initIOContext();
    qint64 seekPacketImpl(qint64 offset, qint32 whence);
    qint32 writePacketImpl(quint8* buf, qint32 bufSize);
    qint32 readPacketImpl(quint8* buf, quint32 bufSize);
    QByteArray serializeMetadata(const CLDeviceVideoLayout* layout);
};

#endif //stream_recorder_h_15_14h
