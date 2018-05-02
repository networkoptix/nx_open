#ifdef ENABLE_DATA_PROVIDERS

#include "proxy_audio_transmitter.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <nx/network/http/custom_headers.h>
#include <common/common_module.h>
#include <network/router.h>
#include <nx/network/http/http_client.h>
#include <nx/streaming/config.h>

namespace
{
    static const int kRequestTimeout = 1000 * 3;

    QUrlQuery toUrlQuery(const QnRequestParams& params)
    {
        QUrlQuery result;
        for (auto itr = params.begin(); itr != params.end(); ++itr)
            result.addQueryItem(itr.key(), itr.value());
        return result;
    }
}

const QByteArray QnProxyAudioTransmitter::kFixedPostRequest(
    "POST /upload_audio HTTP/1.1\r\n"
    "Content-Length: 999999999\r\n\r\n");


QnProxyAudioTransmitter::QnProxyAudioTransmitter(const QnResourcePtr& camera, const QnRequestParams &params):
    m_camera(camera),
    m_initialized(false),
    m_params(params),
    m_sequence(0)
{
    m_initialized = true;
}

QnProxyAudioTransmitter::~QnProxyAudioTransmitter()
{
    stop();
}

void QnProxyAudioTransmitter::endOfRun()
{
    base_type::endOfRun();
    m_socket.reset();
}

bool QnProxyAudioTransmitter::processAudioData(const QnConstCompressedAudioDataPtr& data)
{
    if (!m_socket)
    {
        m_serializer.reset(new QnRtspFfmpegEncoder());

        QnMediaServerResourcePtr mServer = m_camera->getParentResource().dynamicCast<QnMediaServerResource>();
        if (!mServer)
            return false;
        auto currentServer = m_camera->resourcePool()->getResourceById<QnMediaServerResource>(m_camera->commonModule()->moduleGUID());
        if (!currentServer)
            return false;

        auto route = m_camera->commonModule()->router()->routeTo(mServer->getId());
        if (route.addr.isNull())
            return false; //< can't find route
        if (!route.gatewayId.isNull())
            mServer = m_camera->resourcePool()->getResourceById<QnMediaServerResource>(route.gatewayId);
        if (!mServer)
            return false;

        nx::network::http::HttpClient httpClient;
        httpClient.addAdditionalHeader(Qn::SERVER_GUID_HEADER_NAME, m_camera->getParentId().toByteArray());
        httpClient.addAdditionalHeader("Connection", "Keep-Alive");

        nx::utils::Url url;
        url.setScheme("http");
        url.setHost(route.addr.address.toString());
        url.setPort(route.addr.port);
        url.setPath("/proxy-2wayaudio");
        url.setQuery(toUrlQuery(m_params));

        url.setUserName(currentServer->getId().toByteArray());
        url.setPassword(currentServer->getAuthKey());

        httpClient.setResponseReadTimeoutMs(kRequestTimeout);
        httpClient.setSendTimeoutMs(kRequestTimeout);
        if (!httpClient.doPost(url, "text/plain", QByteArray()))
            return false;
        if (httpClient.response()->statusLine.statusCode != nx::network::http::StatusCode::ok)
            return false;

        m_socket = httpClient.takeSocket();

        // send fixed data to be similar to standard HTTP post request
        m_socket->send(kFixedPostRequest);
    }

    m_serializer->setDataPacket(data);
    QnByteArray sendBuffer(CL_MEDIA_ALIGNMENT, 1024 * 64);
    static AVRational r = {1, 1000000};
    AVRational time_base = {1, (int) m_serializer->getFrequency() };

    sendBuffer.resize(4); // reserve space for RTP TCP header
    while(!m_needStop && m_serializer->getNextPacket(sendBuffer))
    {

        const qint64 packetTime = av_rescale_q(data->timestamp, r, time_base);
        QnRtspEncoder::buildRTPHeader(sendBuffer.data() + 4, m_serializer->getSSRC(), m_serializer->getRtpMarker(), packetTime, m_serializer->getPayloadtype(), m_sequence++);

        sendBuffer.data()[0] = '$';
        sendBuffer.data()[1] = 0;
        quint16* lenPtr = (quint16*) (sendBuffer.data() + 2);
        *lenPtr = htons(sendBuffer.size() - 4);

        if (m_socket->send(sendBuffer.constData(), sendBuffer.size()) != sendBuffer.size())
            m_needStop = true;

        sendBuffer.clear();
        sendBuffer.resize(4); // reserve space for RTP TCP header
    }
    if (m_needStop)
        m_socket.reset();

    return true;
}

#endif // ENABLE_DATA_PROVIDERS
