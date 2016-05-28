#ifdef ENABLE_DATA_PROVIDERS

#include "proxy_audio_transmitter.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <http/custom_headers.h>
#include <common/common_module.h>
#include <rtsp/rtsp_ffmpeg_encoder.h>
#include <network/router.h>


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
    m_params(params)
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

bool QnProxyAudioTransmitter::processAudioData(QnConstAbstractMediaDataPtr &data)
{
    if (!m_socket)
    {
        QnMediaServerResourcePtr mServer = m_camera->getParentResource().dynamicCast<QnMediaServerResource>();
        if (!mServer)
            return false;
        auto currentServer = qnResPool->getResourceById<QnMediaServerResource>(qnCommon->moduleGUID());
        if (!currentServer)
            return false;

        auto route = QnRouter::instance()->routeTo(mServer->getId());
        if (!route.gatewayId.isNull())
            mServer = qnResPool->getResourceById<QnMediaServerResource>(route.gatewayId);
        if (!mServer)
            return false;

        nx_http::HttpClient httpClient;
        httpClient.addAdditionalHeader(Qn::SERVER_GUID_HEADER_NAME, m_camera->getParentId().toByteArray());

        QUrl url;
        url.setScheme("http");
        url.setHost(route.addr.address.toString());
        url.setPort(route.addr.port);
        url.setPath("/api/transmitAudio");
        url.setQuery(toUrlQuery(m_params));

        url.setUserName(currentServer->getId().toByteArray());
        url.setPassword(currentServer->getAuthKey());

        httpClient.setResponseReadTimeoutMs(kRequestTimeout);
        httpClient.setSendTimeoutMs(kRequestTimeout);
        if (!httpClient.doPost(url, "text/plain", QByteArray()))
            return false;
        if (httpClient.response()->statusLine.statusCode != nx_http::StatusCode::ok)
            return false;

        m_socket = httpClient.takeSocket();

        // send fixed data to be similar to standard HTTP post request
        m_socket->send(kFixedPostRequest);
    }

    QnRtspFfmpegEncoder serializer;
    serializer.setDataPacket(data);
    QnByteArray sendBuffer(CL_MEDIA_ALIGNMENT, 1024 * 64);

    while(!m_needStop && m_socket->isConnected() && serializer.getNextPacket(sendBuffer))
    {
        quint8 header[2];
        header[0] = sendBuffer.size() >> 8;
        header[1] = (quint8) sendBuffer.size();
        m_socket->send((const char*) &header, 2);

        m_socket->send(sendBuffer);
        sendBuffer.clear();
    }

    return true;
}

#endif // ENABLE_DATA_PROVIDERS
