// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/dataprovider/abstract_media_stream_provider.h>
#include <core/resource/resource_fwd.h>
#include <core/resource/resource_media_layout.h>
#include <nx/media/media_data_packet.h>
#include <nx/streaming/rtsp_stream_provider.h>
#include <utils/camera/camera_diagnostics.h>

namespace nx::streaming {

class NX_VMS_COMMON_API RtspRestreamer:
    public QnAbstractMediaStreamProvider,
    public QnStoppable
{
public:

    RtspRestreamer(
        const QnVirtualCameraResourcePtr& resource,
        const nx::streaming::rtp::TimeOffsetPtr& timeOffset);
    virtual ~RtspRestreamer();

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

    /** Implementation of QnAbstractMediaStreamProvider::getVideoLayout. */
    virtual QnConstResourceVideoLayoutPtr getVideoLayout() const override;

    /** Implementation of QnStoppable::pleaseStop. */
    virtual void pleaseStop() override;

    void setRequest(const QString& request);
    void setRole(Qn::ConnectionRole role);
    nx::utils::Url getCurrentStreamUrl() const;
    void setUserAgent(const QString& value);

    static bool probeStream(
        const char* address,
        const char* login,
        const char* password,
        std::chrono::seconds timeout,
        bool fast = false);

    static const std::string& cloudAddressTemplate();

private:
    static bool tryRewriteRequest(nx::utils::Url& url, bool force);
    static CameraDiagnostics::Result requestToken(
        const nx::utils::Url& rtspUrl,
        const std::string& username,
        const std::string& password,
        std::string& token,
        [[maybe_unused]] std::string& fingerprint);

private:
    nx::streaming::RtspResourceStreamProvider m_reader;
    CameraDiagnostics::Result m_openStreamResult = CameraDiagnostics::NoErrorResult();
    bool m_useCloud = false;
    nx::utils::Url m_url;
    QnVirtualCameraResourcePtr m_camera;
};

} // namespace nx::vms::server
