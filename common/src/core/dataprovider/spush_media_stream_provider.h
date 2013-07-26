#ifndef server_push_stream_reader_h2055
#define server_push_stream_reader_h2055

#include "media_streamdataprovider.h"
#include "../datapacket/media_data_packet.h"
#include "core/dataprovider/live_stream_provider.h"
#include "utils/camera/camera_diagnostics.h"


struct QnAbstractMediaData;

class CLServerPushStreamReader : public QnLiveStreamProvider
{
    Q_OBJECT;

public:
	CLServerPushStreamReader(QnResourcePtr dev );
	virtual ~CLServerPushStreamReader(){stop();}

    virtual bool isStreamOpened() const = 0;
    //!Returns last HTTP response code (even if used media protocol is not http)
    virtual int getLastResponseCode() const { return 0; };

protected:
    virtual CameraDiagnostics::ErrorCode::Value openStream() = 0;
    virtual void closeStream() = 0;
	void pleaseReOpen();
    virtual void afterUpdate() override;
    virtual void beforeRun() override;
    virtual bool canChangeStatus() const;

private:
	virtual void run() override; // in a loop: takes data from device and puts into queue

private:
    bool m_needReopen;
    bool m_cameraAudioEnabled;
};

#endif //server_push_stream_reader_h2055
