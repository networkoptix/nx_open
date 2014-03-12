#ifndef server_push_stream_reader_h2055
#define server_push_stream_reader_h2055

#include <QWaitCondition>

#include "media_streamdataprovider.h"
#include "../datapacket/media_data_packet.h"
#include "core/dataprovider/live_stream_provider.h"


struct QnAbstractMediaData;

class CLServerPushStreamReader : public QnLiveStreamProvider {
    Q_OBJECT

public:
    CLServerPushStreamReader(QnResourcePtr dev );
    virtual ~CLServerPushStreamReader(){stop();}

    virtual bool isStreamOpened() const = 0;
    //!Returns last HTTP response code (even if used media protocol is not http)
    virtual int getLastResponseCode() const { return 0; };

    //!Implementation of QnAbstractMediaStreamDataProvider::diagnoseMediaStreamConnection
    /*!
        Reopens media stream, if it not opened yet.
        Blocks for media stream open attempt
        \return error code and filled \a errorParams with parameters
        \note If stream is opened (\a CLServerPushStreamReader::isStreamOpened() returns true) \a CameraDiagnostics::ErrorCode::noError is returned immediately
    */
    virtual CameraDiagnostics::Result diagnoseMediaStreamConnection() override;

protected:
    virtual CameraDiagnostics::Result openStream() = 0;
    virtual void closeStream() = 0;
    void pleaseReOpen();
    virtual void afterUpdate() override;
    virtual void beforeRun() override;
    virtual bool canChangeStatus() const;

    virtual QnAbstractMediaDataPtr getNextData() = 0;

private:
    virtual void run() override; // in a loop: takes data from device and puts into queue

private:
    bool m_needReopen;
    bool m_cameraAudioEnabled;
    CameraDiagnostics::Result m_openStreamResult;
    //!Incremented with every open stream attempt
    int m_openStreamCounter;
    QWaitCondition m_cond;
    QMutex m_openStreamMutex;
    int m_FrameCnt;
};

#endif //server_push_stream_reader_h2055
