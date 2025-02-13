// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QQueue>

extern "C" {
#include <libavcodec/avcodec.h>
}

#include <export/signer.h>
#include <nx/media/media_data_packet.h>
#include <nx/utils/cryptographic_hash.h>
#include <transcoding/timestamp_corrector.h>

class QnLicensePool;

class NX_VMS_COMMON_API FfmpegMuxer
{
public:
    static const int MTU_SIZE = 1412;

    struct Config
    {
        bool computeSignature = false;
        bool useAbsoluteTimestamp = false;
    };

public:
    FfmpegMuxer(const Config& config);
    ~FfmpegMuxer();

    /*
    * Set ffmpeg container by name
    * @return Returns true if no error or false on error
    */
    bool setContainer(const QString& value);
    QString container() { return m_container; };
    void setFormatOption(const QString& option, const QString& value);
    AVCodecParameters* getVideoCodecParameters() const;
    AVCodecParameters* getAudioCodecParameters() const;
    AVFormatContext* getFormatContext() const { return m_formatCtx; }

    /*
    * Prepare to transcode. If 'direct stream copy' is used, function got not empty video and audio data
    * Destination codecs MUST be used from source data codecs. If 'direct stream copy' is false, video or audio may be empty
    * @return Returns true if no error or false otherwise
    */
    bool open();
    bool addVideo(const AVCodecParameters* codecParameters);
    bool addAudio(const AVCodecParameters* codecParameters);

    /*
    * Process media data and write it to specified nx::utils::ByteArray
    * @param result transcoded data block. If NULL, only decoding is done
    * @return Returns OperationResult::Success if no error or error code otherwise
    */
    bool process(const QnConstAbstractMediaDataPtr& media);
    bool finalize();
    void getResult(nx::utils::ByteArray* const result);

    //!Adds tag to the file. Maximum length of tags and allowed names are format dependent
    /*!
        This implementation always returns \a false
        \return true on success
    */
    bool addTag(const char* name, const char* value);

    QByteArray getSignature(QnLicensePool* licensePool, const nx::Uuid& serverId);

    struct PacketTimestamp
    {
        int64_t ntpTimestamp = 0;
        uint32_t rtpTimestamp = 0;
    };
    PacketTimestamp getLastPacketTimestamp() const { return m_lastPacketTimestamp; }
    void setRtpMtu(int mtu) { m_rtpMtu = mtu; }

    void setPacketizedMode(bool value);
    const QVector<int>& getPacketsSize();
    bool isCodecSupported(AVCodecID id) const;
    void setStartTimeOffset(int64_t value);

public:
    // For internal use only, move to protected!
     // TODO replace by ffmpeg::IoContext
    int writeBuffer(const char* data, int size);
    void setInMiddleOfStream(bool value) { m_inMiddleOfStream = value; }
    bool inMiddleOfStream() const { return m_inMiddleOfStream; }

private:
    AVIOContext* createFfmpegIOContext();
    void closeFfmpegContext();
    bool muxPacket(const QnConstAbstractMediaDataPtr& mediaPacket);
    void checkDiscontinuity(const QnConstAbstractMediaDataPtr& data, int streamIndex);

private:
    Config m_config;
    nx::utils::ByteArray m_internalBuffer;
    QVector<int> m_outputPacketSize;

    bool m_initialized = false;
    bool m_initializedAudio = false;    // Incoming audio packets will be ignored.
    bool m_initializedVideo = false;    // Incoming video packets will be ignored.
    bool m_packetizedMode = false;

    MediaSigner m_mediaSigner;
    AVCodecParameters* m_videoCodecParameters = nullptr;
    AVCodecParameters* m_audioCodecParameters = nullptr;
    AVFormatContext* m_formatCtx = nullptr;

    QString m_container;
    TimestampCorrector m_timestampCorrector;
    PacketTimestamp m_lastPacketTimestamp;
    bool m_inMiddleOfStream = false;
    int m_rtpMtu = MTU_SIZE;
};
