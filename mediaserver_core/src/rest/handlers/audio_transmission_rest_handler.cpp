#include "audio_transmission_rest_handler.h"
#include <streaming/audio_streamer_pool.h>
#include <utils/network/http/httptypes.h>
#include <rest/server/rest_connection_processor.h>
#include <core/dataprovider/spush_media_stream_provider.h>
#include <network/ffmpeg_rtp_parser.h>
#include <2wayaudio/proxy_audio_transmitter.h>
#include <utils/common/from_this_to_shared.h>

namespace
{
    const QString kClientIdParamName("clientId");
    const QString kResourceIdParamName("resourceId");
    const QString kActionParamName("action");
    const QString kStartStreamAction("start");
    const QString kStopStreamAction("stop");

    bool readBytes(QSharedPointer<AbstractStreamSocket> socket, quint8* dst, int size)
    {
        while (size > 0)
        {
            int readed = socket->recv(dst, size);
            if (readed < 1)
                return false;
            dst += readed;
            size -= readed;
        }
        return true;
    }
}

QMutex QnAudioTransmissionRestHandler::m_mutex;
QMap<QString, QSharedPointer<QnProxyDesktopDataProvider>> QnAudioTransmissionRestHandler::m_proxyProviders;


int QnAudioTransmissionRestHandler::executeGet(
    const QString &path,
    const QnRequestParams &params,
    QnJsonRestResult &result,
    const QnRestConnectionProcessor* )
{
    QN_UNUSED(path);

    QString errorStr;
    if (!validateParams(params, errorStr))
    {
        result.setError(QnJsonRestResult::InvalidParameter, errorStr);
        return nx_http::StatusCode::ok;
    }

    auto clientId = params[kClientIdParamName];
    auto resourceId = params[kResourceIdParamName];
    QnAudioStreamerPool::Action action = (params[kActionParamName] == kStartStreamAction)
        ? QnAudioStreamerPool::Action::Start
        : QnAudioStreamerPool::Action::Stop;

    if (!QnAudioStreamerPool::instance()->startStopStreamToResource(clientId, resourceId, action, errorStr, params))
        result.setError(QnJsonRestResult::CantProcessRequest, errorStr);
    return nx_http::StatusCode::ok;
}

class QnProxyDesktopDataProvider:
    public CLServerPushStreamReader
{
private:
    quint8* m_recvBuffer;
    QnFfmpegRtpParser m_parser;

public:
    QnProxyDesktopDataProvider():
        CLServerPushStreamReader(QnResourcePtr())
    {
        m_recvBuffer = new quint8[65536];
    }

    virtual ~QnProxyDesktopDataProvider()
    {
        stop();
        delete [] m_recvBuffer;
    }

    void setSocket(const QSharedPointer<AbstractStreamSocket>& socket)
    {
        QMutexLocker lock(&m_mutex);
        m_socket = socket;
    }

protected:

    virtual void closeStream()  override
    {
        QMutexLocker lock(&m_mutex);
        m_socket->close();
    }

    virtual bool isStreamOpened() const
    {
        QMutexLocker lock(&m_mutex);
        return m_socket->isConnected();
    }

    virtual CameraDiagnostics::Result openStreamInternal(bool isCameraControlRequired, const QnLiveStreamParams& params) override
    {
        return CameraDiagnostics::NoErrorResult();
    }

    virtual QnAbstractMediaDataPtr getNextData() override
    {
        QMutexLocker lock(&m_mutex);
        while (!m_needStop && m_socket->isConnected())
        {
            if (!readBytes(m_socket, m_recvBuffer, 2))
            {
                m_socket->close();
                break;
            }

            int packetSize = (m_recvBuffer[0] << 8) + m_recvBuffer[1];
            if (!readBytes(m_socket, m_recvBuffer, packetSize))
            {
                m_socket->close();
                break;
            }

            bool gotData;
            m_parser.processData(m_recvBuffer, 0, packetSize, RtspStatistic(), gotData);
            if (gotData)
                return m_parser.nextData();
        }
        return QnAbstractMediaDataPtr();
    }
private:
    QSharedPointer<AbstractStreamSocket> m_socket;
    mutable QMutex m_mutex;
};



bool QnAudioTransmissionRestHandler::readHttpHeaders(QSharedPointer<AbstractStreamSocket> socket)
{
    int toRead = QnProxyAudioTransmitter::kFixedPostRequest.size();
    QByteArray buffer;
    buffer.resize(toRead);
    return readBytes(socket, (quint8*) buffer.data(), toRead);
}

void QnAudioTransmissionRestHandler::afterExecute(const nx_http::Request& request, const QnRequestParamList &_params, const QByteArray& body, const QnRestConnectionProcessor* owner)
{
    if (request.requestLine.method != "POST")
        return;

    QnRequestParams params;
    for(const QnRequestParam &param: _params)
        params.insertMulti(param.first, param.second);

    QnJsonRestResult reply;
    if (!QJson::deserialize(body, &reply) || reply.error !=  QnJsonRestResult::NoError)
        return;

    const QnUuid clientId = params[kClientIdParamName];

    auto resourceId = params[kResourceIdParamName];
    QnAudioStreamerPool::Action action = (params[kActionParamName] == kStartStreamAction)
        ? QnAudioStreamerPool::Action::Start
        : QnAudioStreamerPool::Action::Stop;

    // process 2-nd POST request with unlimited length

    QSharedPointer<AbstractStreamSocket> tcpSocket = const_cast<QnRestConnectionProcessor*>(owner)->takeSocket();
    if (!readHttpHeaders(tcpSocket))
        return;

    //QSharedPointer<QnProxyDesktopDataProvider> desktopDataProvider(new QnProxyDesktopDataProvider(tcpSocket));
    QSharedPointer<QnProxyDesktopDataProvider> desktopDataProvider;
    {
        QMutexLocker lock(&m_mutex);
        QString key = clientId.toString() + resourceId;
        auto itr = m_proxyProviders.find(key);
        if (itr == m_proxyProviders.end())
            itr = m_proxyProviders.insert(key, QSharedPointer<QnProxyDesktopDataProvider>(new QnProxyDesktopDataProvider()));
        desktopDataProvider = itr.value();
        desktopDataProvider->setSocket(tcpSocket);
    }

    QString errString;
    if (!QnAudioStreamerPool::instance()->startStopStreamToResource(desktopDataProvider, resourceId, action, errString))
        qWarning() << "Cant start audio uploading to camera" << resourceId << errString;
}

int QnAudioTransmissionRestHandler::executePost(const QString &path, const QnRequestParams &params, const QByteArray &body, QnJsonRestResult &result, const QnRestConnectionProcessor* owner)
{
    QString errorStr;
    if (!validateParams(params, errorStr))
        result.setError(QnJsonRestResult::InvalidParameter, errorStr);
    return nx_http::StatusCode::ok;
}

bool QnAudioTransmissionRestHandler::validateParams(const QnRequestParams &params, QString& error) const
{
    bool ok = true;
    if (!params.contains(kClientIdParamName))
    {
        ok = false;
        error += lit("Client ID is not specified. ");
    }
    if (!params.contains(kResourceIdParamName))
    {
        ok = false;
        error += lit("Resource ID is not specified. ");
    }
    if (!params.contains(kActionParamName))
    {
        ok = false;
        error += lit("Action is not specified. ");
        return false;
    }

    auto action = params[kActionParamName];

    if (action != kStartStreamAction && action != kStopStreamAction)
    {
        ok = false;
        error += lit("Action parameter should has value of either '%1' or '%2'")
                .arg(kStartStreamAction)
                .arg(kStopStreamAction);
    }

    return ok;

}
