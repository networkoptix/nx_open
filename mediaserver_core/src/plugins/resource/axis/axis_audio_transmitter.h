#pragma once

#include "core/dataconsumer/audio_data_transmitter.h"
#include "core/resource/security_cam_resource.h"
#include <utils/network/system_socket.h>
#include "../../../common/src/transcoding/ffmpeg_audio_transcoder.h"

class QnAxisAudioTransmitter : public QnAbstractAudioTransmitter
{
    Q_OBJECT
public:
    QnAxisAudioTransmitter(QnSecurityCamResource* res, CodecID codecId);
    ~QnAxisAudioTransmitter();

    virtual bool processAudioData(QnConstAbstractMediaDataPtr &data) override;

    static QSet<CodecID> getSupportedAudioCodecs();

private:
    bool initSocket();
    bool establishConnection();
    SocketAddress getSocketAddress() const;
    QByteArray buildTransmitRequest() const;
    QString getAudioMimeType(CodecID codecId) const;


private:
    QnSecurityCamResource* m_resource;
    CodecID m_codecId;
    QnFfmpegAudioTranscoder m_transcoder;
    bool m_audioContextOpened;
    bool m_connectionEstablished;
    bool m_canProcessData;
    std::shared_ptr<TCPSocket> m_socket;
    QnMutex m_connectionMutex;
};
