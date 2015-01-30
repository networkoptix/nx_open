#ifndef server_push_stream_reader_h2055
#define server_push_stream_reader_h2055

#ifdef ENABLE_DATA_PROVIDERS

#include <QWaitCondition>

#include "abstract_media_stream_provider.h"
#include "core/dataprovider/live_stream_provider.h"


struct QnAbstractMediaData;

class CLServerPushStreamReader
:
    public QnLiveStreamProvider,
    public QnAbstractMediaStreamProvider
{
    Q_OBJECT

public:
    CLServerPushStreamReader(const QnResourcePtr& dev );
    virtual ~CLServerPushStreamReader(){stop();}

    //!Implementation of QnAbstractMediaStreamDataProvider::diagnoseMediaStreamConnection
    /*!
        Reopens media stream, if it not opened yet.
        Blocks for media stream open attempt
        \return error code and filled \a errorParams with parameters
        \note If stream is opened (\a CLServerPushStreamReader::isStreamOpened() returns true) \a CameraDiagnostics::ErrorCode::noError is returned immediately
    */
    virtual CameraDiagnostics::Result diagnoseMediaStreamConnection() override;
    virtual bool isCameraControlRequired() const override;
protected:
    void pleaseReOpen();
    virtual void afterUpdate() override;
    virtual void beforeRun() override;
    virtual bool canChangeStatus() const;

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
    QElapsedTimer m_needControlTimer;
private:
    CameraDiagnostics::Result openStreamInternal();
private:
    bool m_cameraControlRequired;
};

#endif // ENABLE_DATA_PROVIDERS

#endif //server_push_stream_reader_h2055
