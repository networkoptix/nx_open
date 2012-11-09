#ifndef server_push_stream_reader_h2055
#define server_push_stream_reader_h2055


#include "media_streamdataprovider.h"
#include "../datapacket/media_data_packet.h"


struct QnAbstractMediaData;

class CLServerPushStreamreader : public QnAbstractMediaStreamDataProvider
{
    Q_OBJECT;

public:
	CLServerPushStreamreader(QnResourcePtr dev );
	virtual ~CLServerPushStreamreader(){stop();}

    virtual bool isStreamOpened() const = 0;
protected:
	
    virtual void openStream() = 0;
    virtual void closeStream() = 0;
	void pleaseReOpen();
    virtual void afterUpdate() override;
private:
	void run(); // in a loop: takes data from device and puts into queue
    virtual void beforeRun() override;
    bool canChangeStatus() const;
private:
    bool m_needReopen;
    bool m_cameraAudioEnabled;
};

#endif //server_push_stream_reader_h2055
