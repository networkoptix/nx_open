#ifndef server_push_stream_reader_h2055
#define server_push_stream_reader_h2055


#include "media_streamdataprovider.h"
#include "../datapacket/media_data_packet.h"
#include "core/dataprovider/live_stream_provider.h"


struct QnAbstractMediaData;

class CLServerPushStreamreader : public QnLiveStreamProvider
{
    Q_OBJECT;

public:
	CLServerPushStreamreader(QnResourcePtr dev );
	virtual ~CLServerPushStreamreader(){stop();}

    virtual bool isStreamOpened() const = 0;
    //!Returns last HTTP response code (even if used media protocol is not http)
    virtual int getLastResponseCode() const { return 0; };
protected:
	
    virtual void openStream() = 0;
    virtual void closeStream() = 0;
	void pleaseReOpen();
    virtual void afterUpdate() override;
    virtual void beforeRun() override;

private:
	void run(); // in a loop: takes data from device and puts into queue
    bool canChangeStatus() const;
private:
    bool m_needReopen;
    bool m_cameraAudioEnabled;
};

#endif //server_push_stream_reader_h2055
