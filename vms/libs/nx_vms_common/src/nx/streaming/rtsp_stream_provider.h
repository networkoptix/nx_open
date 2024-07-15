#pragma once

#include <atomic>
#include <vector>

#include <QtCore/QElapsedTimer>

#include <common/common_globals.h>
#include <core/dataprovider/abstract_media_stream_provider.h>
#include <core/dataprovider/live_stream_params.h>
#include <core/resource/resource_media_layout.h>
#include <nx/media/media_data_packet.h>
#include <nx/network/multicast_address_registry.h>
#include <nx/rtp/parsers/rtp_stream_parser.h>
#include <nx/streaming/rtp/camera_time_helper.h>
#include <nx/streaming/rtsp_client.h>
#include <nx/utils/log/format.h>
#include <nx/utils/safe_direct_connection.h>
#include <nx/utils/thread/stoppable.h>
#include <nx/vms/common/system_settings.h>
#include <nx/vms/event/event_fwd.h>
#include <utils/camera/camera_diagnostics.h>

namespace nx::vms::rules { struct NetworkIssueInfo; }

namespace nx::rtp {

class RtpParser;
class IRtpParserFactory;

} // namespace nx::rtp

namespace nx::streaming {


class NX_VMS_COMMON_API RtspStreamProvider:
    public QnAbstractMediaStreamProvider,
    public QnStoppable
{
public:
    using OnSocketReadTimeoutCallback = nx::utils::MoveOnlyFunc<QnAbstractMediaDataPtr()>;

    RtspStreamProvider(
        const nx::vms::common::SystemSettings* systemSetting,
        const nx::streaming::rtp::TimeOffsetPtr& timeOffset);
    virtual ~RtspStreamProvider();

    /** Implementation of QnAbstractMediaStreamProvider::getNextData. */
    virtual QnAbstractMediaDataPtr getNextData() override;

    /** Implementation of QnAbstractMediaStreamProvider::openStream. */
    virtual CameraDiagnostics::Result openStream() override;

    /** Implementation of QnAbstractMediaStreamProvider::closeStream. */
    virtual void closeStream() override;

    /** Implementation of QnAbstractMediaStreamProvider::isStreamOpened. */
    virtual bool isStreamOpened() const override;
    virtual CameraDiagnostics::Result lastOpenStreamResult() const override;

    /** Implementation of QnAbstractMediaStreamProvider::getAudioLayout. */
    virtual AudioLayoutConstPtr getAudioLayout() const override;

    /** Implementation of QnStoppable::pleaseStop. */
    virtual void pleaseStop() override;

    void setRequest(const QString& request);
    void setRole(Qn::ConnectionRole role);
    void setPreferredAuthScheme(const nx::network::http::header::AuthScheme::Value scheme);

    static void setDefaultTransport(nx::vms::api::RtpTransportType defaultTransportToUse);
    void setRtpTransport(nx::vms::api::RtpTransportType value);

    virtual QnConstResourceVideoLayoutPtr getVideoLayout() const override;
    void setUserAgent(const QString& value);

    nx::utils::Url getCurrentStreamUrl() const;

    void setPlaybackRange(int64_t startTimeUsec, int64_t endTimeUsec = AV_NOPTS_VALUE);

    void setDateTimeFormat(const QnRtspClient::DateTimeFormat& format);

    QnRtspClient& rtspClient();
    void addRequestHeader(const QString& requestName, const nx::network::http::HttpHeader& header);
    void setRtpFrameTimeoutMs(int value);

    /**
     * This callback is called every time socket read timeout happens.
     * Connection may be closed on socket read error, media frame timeout
     * or by RTSP session timeout (usually 1 minute).
     * If callback returns valid packet it is returned from getNextDataTcp, otherwise timeout is skipped.
     */
    void setOnSocketReadTimeoutCallback(
        std::chrono::milliseconds timeout,
        OnSocketReadTimeoutCallback callback);

    virtual nx::vms::api::RtpTransportType getRtpTransport() const;

    // Adds custom track parser factory to the chain
    void addCustomTrackParserFactory(
        std::unique_ptr<nx::rtp::IRtpParserFactory> parserFactory);

    // Extra payload types for which parsers will be created for each track, if possible
    void setExtraPayloadTypes(std::vector<int> payloadTypes);

    // Enable connect proxying via Cloud.
    void setCloudConnectEnabled(bool value);

    std::chrono::microseconds translateTimestampFromCameraToVmsSystem(
        std::chrono::microseconds timestamp,
        int channelNumber);

    QAuthenticator credentials() const;
    void setCredentials(const QAuthenticator& credentials);

    /*
     * Never correct RTSP time, trust RTSP source.
     */
    void setForceCameraTime(bool value);

    void setUrl(const nx::utils::Url& url);

protected:
    const QString& logName() const { return m_logName; }

    virtual nx::network::MulticastAddressRegistry* multicastAddressRegistry();
    virtual QnAdvancedStreamParams advancedLiveStreamParams() const;
    virtual void reportNetworkIssue(
        std::chrono::microseconds timestamp,
        nx::vms::api::EventReason reasonCode,
        const nx::vms::rules::NetworkIssueInfo& info);
    virtual int numberOfVideoChannels() const;
    virtual void at_numberOfVideoChannelsChanged() {}
    virtual bool isRtcpReportsForced() const;
    virtual std::string timeHelperKey() const;
    virtual void updateStreamUrlIfNeeded() {};
    virtual nx::streaming::rtp::TimePolicy getTimePolicy() const;
    virtual CameraDiagnostics::Result registerMulticastAddressesIfNeeded();

private:
    struct TrackInfo
    {
        QnRtspIoDevice* ioDevice = nullptr; //< External reference; do not delete.
        // Separate parsers for different payloads
        // Usually there will be only one
        std::map<int, std::unique_ptr<nx::rtp::RtpParser>> rtpParsers;
        int rtcpChannelNumber = 0;
        int logicalChannelNum = 0;
        std::unique_ptr<nx::streaming::rtp::CameraTimeHelper> timeHelper;

        TrackInfo() = default;
        TrackInfo(TrackInfo&&) = default;
    };

    nx::rtp::StreamParserPtr createParser(const QString& codecName);
    nx::rtp::StreamParserPtr createParser(int payloadType);
    bool gotKeyData(const QnAbstractMediaDataPtr& mediaData);
    void clearKeyData(int channelNum);
    QnAbstractMediaDataPtr getNextDataUDP();
    QnAbstractMediaDataPtr getNextDataTCP();
    void processTcpRtcp(
        quint8* buffer, int bufferSize, int bufferCapacity);
    void buildClientRTCPReport(quint8 chNumber);
    QnAbstractMediaDataPtr getNextDataInternal();
    void processCameraTimeHelperEvent(nx::streaming::rtp::CameraTimeHelper::EventType event);

    bool isFormatSupported(const nx::rtp::Sdp::Media media) const;
    bool processData(
        TrackInfo& track, uint8_t* buffer, int offset, int size, int channel, int& errorCount);

    // Returns true if any of custom track factories support this codec
    bool isCodecSupportedByCustomParserFactories(const QString& codecName) const;
    bool isDataTimerExpired() const;

    void createTrackParsers();

protected:
    static nx::Mutex& defaultTransportMutex();

protected:
    QnRtspClient m_RtpSession;
    nx::streaming::rtp::TimeOffsetPtr m_timeOffset;
    std::vector<bool> m_gotKeyDataInfo;
    std::vector<TrackInfo> m_tracks;
    mutable nx::Mutex m_tracksMutex;
    std::map<int, int> m_trackIndices;

    QString m_request;
    QString m_logName;

    std::vector<nx::utils::ByteArray*> m_demuxedData;
    int m_numberOfVideoChannels;
    bool m_ignoreRtcpReports = false;
    bool m_pleaseStop;
    QElapsedTimer m_rtcpReportTimer;
    bool m_gotSomeFrame;
    Qn::ConnectionRole m_role;

    QnCustomResourceVideoLayoutPtr m_customVideoLayout;
    mutable nx::Mutex m_layoutMutex;
    AudioLayoutConstPtr m_audioLayout;
    bool m_gotData;
    QElapsedTimer m_dataTimer;
    nx::network::http::header::AuthScheme::Value m_preferredAuthScheme;
    nx::vms::api::RtpTransportType m_rtpTransport;

    int m_maxRtpRetryCount{0};
    int m_rtpFrameTimeoutMs{0};
    bool m_forceCameraTime = false;

    struct PlaybackRange
    {
        PlaybackRange() {};

        PlaybackRange(int64_t startTimeUsec, int64_t endTimeUsec):
            startTimeUsec(startTimeUsec),
            endTimeUsec(endTimeUsec)
        {
        }

        int64_t startTimeUsec = AV_NOPTS_VALUE;
        int64_t endTimeUsec = AV_NOPTS_VALUE;

        bool isValid() const
        {
            if (startTimeUsec == AV_NOPTS_VALUE)
                return false;

            if (endTimeUsec == AV_NOPTS_VALUE)
                return true;

            return endTimeUsec >= startTimeUsec;
        }

        QString toString() const
        {
            return nx::format("(%1us, %2us)", startTimeUsec, endTimeUsec);
        }
    };

    PlaybackRange m_playbackRange;
    OnSocketReadTimeoutCallback m_onSocketReadTimeoutCallback;
    std::chrono::milliseconds m_callbackTimeout{0};
    CameraDiagnostics::Result m_openStreamResult;
    std::optional<std::chrono::steady_clock::time_point> m_packetLossReportTime;

    mutable nx::Mutex m_mutex;
    static nx::vms::api::RtpTransportType s_defaultTransportToUse;
    std::set<nx::network::MulticastAddressRegistry::RegisteredAddressHolderPtr>
        m_registeredMulticastAddresses;

    std::vector<std::unique_ptr<nx::rtp::IRtpParserFactory>> m_customParserFactories;
    std::vector<int> m_extraPayloadTypes;
    nx::rtp::Result m_lastRtpParseResult;
    nx::network::AbstractStreamSocket* m_tcpSocket = nullptr;
    bool m_cloudConnectEnabled = false;
    nx::utils::Url m_url;
    QAuthenticator m_credentials;
};

class NX_VMS_COMMON_API RtspResourceStreamProvider: public RtspStreamProvider
{
    using base_type = RtspStreamProvider;
public:
    RtspResourceStreamProvider(
        const QnVirtualCameraResourcePtr& resource,
        const nx::streaming::rtp::TimeOffsetPtr& timeOffset);

    virtual CameraDiagnostics::Result openStream() override;
    virtual nx::vms::api::RtpTransportType getRtpTransport() const override;

protected:
    QnVirtualCameraResourcePtr resource() const { return m_resource; }
    void updateTimePolicy();
    virtual int numberOfVideoChannels() const override;
    virtual void at_numberOfVideoChannelsChanged() override;
    virtual bool isRtcpReportsForced() const override;
    virtual std::string timeHelperKey() const override;
    virtual void updateStreamUrlIfNeeded() override;
    virtual nx::streaming::rtp::TimePolicy getTimePolicy() const override;
    virtual CameraDiagnostics::Result registerMulticastAddressesIfNeeded() override;

private:
    CameraDiagnostics::Result registerAddressIfNeeded(
        const QnRtspIoDevice::AddressInfo& addressInfo);

private:
    QnVirtualCameraResourcePtr m_resource;
};

} // namespace nx::streaming
