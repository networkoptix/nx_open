#include "stream_reader.h"

#if defined(_WIN32)
    #include <Windows.h>
    #undef max
    #undef min
#else
    #include <time.h>
    #include <unistd.h>
#endif

#include <sys/timeb.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <algorithm>
#include <fstream>
#include <functional>
#include <memory>

#include <QtCore/QElapsedTimer>

#include <nx/utils/std/cpp14.h>
#include <nx/utils/log/log.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/byte_stream/custom_output_stream.h>

#include "ilp_empty_packet.h"
#include "motion_data_picture.h"
#include "plugin.h"

static const nxcip::UsecUTCTimestamp USEC_IN_MS = 1000;
static const nxcip::UsecUTCTimestamp USEC_IN_SEC = 1000 * 1000;
static const nxcip::UsecUTCTimestamp NSEC_IN_USEC = 1000;
static const int MAX_FRAME_SIZE = 4*1024*1024;

namespace nx::vms_server_plugins::mjpeg_link {

StreamReader::StreamReader(
    nxpt::CommonRefManager* const parentRefManager,
    nxpl::TimeProvider *const timeProvider,
    const QString& login,
    const QString& password,
    const QString& url,
    float fps,
    int encoderNumber)
    :
    m_refManager(parentRefManager),
    m_login(login),
    m_password(password),
    m_url(url),
    m_fps(fps),
    m_encoderNumber(encoderNumber),
    m_curTimestamp(0),
    m_streamType(none),
    m_prevFrameClock(-1),
    m_frameDurationMSec(0),
    m_terminated(false),
    m_isInGetNextData(0),
    m_timeProvider(timeProvider)
{
    NX_ASSERT(m_timeProvider);
    setFps(fps);
}

StreamReader::~StreamReader()
{
    NX_ASSERT(m_isInGetNextData == 0);
    m_timeProvider->releaseRef();

    m_videoPacket.reset();
}

void* StreamReader::queryInterface(const nxpl::NX_GUID& interfaceID)
{
    if (memcmp(&interfaceID, &nxcip::IID_StreamReader, sizeof(nxcip::IID_StreamReader)) == 0)
    {
        addRef();
        return this;
    }
    if (memcmp(&interfaceID, &nxpl::IID_PluginInterface, sizeof(nxpl::IID_PluginInterface)) == 0)
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    return NULL;
}

int StreamReader::addRef() const
{
    return m_refManager.addRef();
}

int StreamReader::releaseRef() const
{
    return m_refManager.releaseRef();
}

static const unsigned int MAX_FRAME_DURATION_MS = 5000;

int StreamReader::getNextData(nxcip::MediaDataPacket** lpPacket)
{
    ++m_isInGetNextData;
    auto SCOPED_GUARD_FUNC = [this](StreamReader*) { --m_isInGetNextData; };
    const std::unique_ptr<StreamReader, decltype(SCOPED_GUARD_FUNC)>
        SCOPED_GUARD(this, SCOPED_GUARD_FUNC);

    bool httpClientHasBeenJustCreated = false;
    std::shared_ptr<nx::network::http::HttpClient> localHttpClientPtr;
    {
        QnMutexLocker lk(&m_mutex);
        if (!m_httpClient)
        {
            m_httpClient = std::make_shared<nx::network::http::HttpClient>();
            m_httpClient->setMessageBodyReadTimeout(
                std::chrono::milliseconds(MAX_FRAME_DURATION_MS));
            httpClientHasBeenJustCreated = true;
        }
        localHttpClientPtr = m_httpClient;
    }

    if (httpClientHasBeenJustCreated)
    {
        using namespace std::placeholders;

        m_multipartContentParser = std::make_unique<nx::network::http::MultipartContentParser>();
        auto jpgFrameHandleFunc = std::bind(&StreamReader::gotJpegFrame, this, _1);
        m_multipartContentParser->setNextFilter(
            std::make_shared<nx::utils::bstream::CustomOutputStream<decltype(jpgFrameHandleFunc)>>(
                jpgFrameHandleFunc));

        const int result = doRequest(localHttpClientPtr.get());
        if (result != nxcip::NX_NO_ERROR)
            return result;

        if (nx::network::http::strcasecmp(localHttpClientPtr->contentType(), "image/jpeg") == 0)
        {
            // Single jpeg, have to get it by timer.
            m_streamType = jpg;
        }
        else if (m_multipartContentParser->setContentType(localHttpClientPtr->contentType()))
        {
            // Motion jpeg stream.
            m_streamType = mjpg;
        }
        else
        {
            NX_DEBUG(this, "Unsupported Content-Type %1", localHttpClientPtr->contentType());
            return nxcip::NX_UNSUPPORTED_CODEC;
        }
    }

    switch (m_streamType)
    {
        case jpg:
        {
            if (!httpClientHasBeenJustCreated)
            {
                if (!waitForNextFrameTime())
                {
                    NX_DEBUG(this, "Interrupted");
                    return nxcip::NX_INTERRUPTED;
                }

                const int result = doRequest(localHttpClientPtr.get());
                if (result != nxcip::NX_NO_ERROR)
                {
                    NX_DEBUG(this, "Error %1", result);
                    return result;
                }
            }

            nx::network::http::BufferType msgBody;
            while (msgBody.size() < MAX_FRAME_SIZE)
            {
                const nx::network::http::BufferType& msgBodyBuf =
                    localHttpClientPtr->fetchMessageBodyBuffer();
                {
                    QnMutexLocker lk(&m_mutex);
                    if (m_terminated)
                    {
                        NX_DEBUG(this, "Terminated");
                        m_terminated = false;
                        return nxcip::NX_INTERRUPTED;
                    }
                }
                if (localHttpClientPtr->eof())
                {
                    NX_DEBUG(this, "End of stream");
                    gotJpegFrame(msgBody.isEmpty() ? msgBodyBuf : (msgBody + msgBodyBuf));
                    localHttpClientPtr.reset();
                    break;
                }
                else
                {
                    msgBody += msgBodyBuf;
                }
            }
            break;
        }

        case mjpg:
            // Reading mjpg picture.
            while (!m_videoPacket.get() && !localHttpClientPtr->eof())
            {
                m_multipartContentParser->processData(
                    localHttpClientPtr->fetchMessageBodyBuffer());
                {
                    QnMutexLocker lk(&m_mutex);
                    if (m_terminated)
                    {
                        NX_DEBUG(this, "Terminated");
                        m_terminated = false;
                        return nxcip::NX_INTERRUPTED;
                    }
                }
            }
            break;

        default:
            break;
    }

    if (m_videoPacket.get())
    {
        *lpPacket = m_videoPacket.release();
        const auto plugin = HttpLinkPlugin::instance();
        if (!plugin)
        {
            NX_DEBUG(this, "No plugin");
            return nxcip::NX_OTHER_ERROR;
        }

        plugin->setStreamState(m_url, /*isStreamRunning*/ true);
        return nxcip::NX_NO_ERROR;
    }

    QnMutexLocker lk(&m_mutex);
    if (m_httpClient)
    {
        //reconnecting
        m_httpClient.reset();
    }

    NX_DEBUG(this, "Stream has ended");
    return nxcip::NX_NETWORK_ERROR;
}

void StreamReader::interrupt()
{
    std::shared_ptr<nx::network::http::HttpClient> client;
    {
        QnMutexLocker lk(&m_mutex);
        m_terminated = true;
        m_cond.wakeAll();
        std::swap(client, m_httpClient);
    }

    // Closing connection.
    if (client)
    {
        client->pleaseStop();
        client.reset();
    }
}

void StreamReader::setFps(float fps)
{
    m_fps = fps;
    m_frameDurationMSec = (qint64) (1000.0 / m_fps);
}

void StreamReader::updateCredentials(const QString& login, const QString& password)
{
    m_login = login;
    m_password = password;
}

void StreamReader::updateMediaUrl(const QString& url)
{
    m_url = url;
}

QString StreamReader::idForToStringFromPtr() const
{
    return m_url;
}

int StreamReader::doRequest(nx::network::http::HttpClient* const httpClient)
{
    httpClient->setUserName(m_login);
    httpClient->setUserPassword(m_password);
    if (!httpClient->doGet(nx::utils::Url(m_url)) || !httpClient->response())
    {
        NX_DEBUG(this, "Failed to request stream");
        return nxcip::NX_NETWORK_ERROR;
    }
    if (httpClient->response()->statusLine.statusCode
        == nx::network::http::StatusCode::unauthorized)
    {
        NX_DEBUG(this, "Failed to request stream: %1",
            httpClient->response()->statusLine.reasonPhrase);
        return nxcip::NX_NOT_AUTHORIZED;
    }
    if (httpClient->response()->statusLine.statusCode / 100 * 100
        != nx::network::http::StatusCode::ok)
    {
        NX_DEBUG(this, "Failed to request stream: %1",
            httpClient->response()->statusLine.reasonPhrase);
        return nxcip::NX_NETWORK_ERROR;
    }
    NX_DEBUG(this, "Stream is opened successfuly");
    return nxcip::NX_NO_ERROR;
}

void StreamReader::gotJpegFrame(const nx::network::http::ConstBufferRefType& jpgFrame)
{
    // Creating video packet.
    m_videoPacket.reset(new ILPVideoPacket(
        &m_allocator,
        0,
        m_timeProvider->millisSinceEpoch() * USEC_IN_MS,
        nxcip::MediaDataPacket::fKeyPacket,
        0));
    m_videoPacket->resizeBuffer(jpgFrame.size());
    if (m_videoPacket->data())
        memcpy(m_videoPacket->data(), jpgFrame.constData(), jpgFrame.size());
}

bool StreamReader::waitForNextFrameTime()
{
    const qint64 currentTime = m_timeProvider->millisSinceEpoch();
    if (m_prevFrameClock != -1 &&
        // System time changed.
        !((m_prevFrameClock > currentTime)
            || (currentTime - m_prevFrameClock > m_frameDurationMSec))) 
    {
        const qint64 msecToSleep = m_frameDurationMSec - (currentTime - m_prevFrameClock);
        if (msecToSleep > 0)
        {
            QnMutexLocker lk(&m_mutex);
            QElapsedTimer monotonicTimer;
            monotonicTimer.start();
            qint64 msElapsed = monotonicTimer.elapsed();
            while (!m_terminated && (msElapsed < msecToSleep))
            {
                m_cond.wait(lk.mutex(), msecToSleep - msElapsed);
                msElapsed = monotonicTimer.elapsed();
            }
            if (m_terminated)
            {
                // The call has been interrupted.
                m_terminated = false;
                return false;
            }
        }
    }
    m_prevFrameClock = m_timeProvider->millisSinceEpoch();
    return true;
}

} // namespace nx::vms_server_plugins::mjpeg_link
