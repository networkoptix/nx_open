#ifndef server_push_stream_reader_h2055
#define server_push_stream_reader_h2055

#ifdef ENABLE_DATA_PROVIDERS

#include <QtCore/QElapsedTimer>

#include <nx/utils/thread/wait_condition.h>

#include <core/dataprovider/abstract_media_stream_provider.h>
#include "live_stream_provider.h"


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

private slots:
    void at_resourceChanged(const QnResourcePtr& res);

protected:
    QnLiveStreamParams m_currentLiveParams;

    virtual void pleaseReopenStream() override;
    virtual void beforeRun() override;
    virtual void afterRun() override;
    virtual bool canChangeStatus() const;
    virtual CameraDiagnostics::Result openStreamInternal(bool isCameraControlRequired, const QnLiveStreamParams& params) = 0;

private:
    virtual CameraDiagnostics::Result openStream() override final;
    virtual void run() override final; // in a loop: takes data from device and puts into queue
    CameraDiagnostics::Result openStreamWithErrChecking(bool forceStreamCtrl);
    virtual bool isCameraControlRequired() const;

private:
    bool m_needReopen;
    bool m_cameraAudioEnabled;
    CameraDiagnostics::Result m_openStreamResult;
    //!Incremented with every open stream attempt
    int m_openStreamCounter;
    QnWaitCondition m_cond;
    QnMutex m_openStreamMutex;
    int m_FrameCnt;
    QElapsedTimer m_needControlTimer;

private:
    bool m_openedWithStreamCtrl;
};

#endif // ENABLE_DATA_PROVIDERS

#endif //server_push_stream_reader_h2055
