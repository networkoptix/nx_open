// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "webrtc_receiver.h"

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include "webrtc_demuxer.h"
#include "webrtc_session.h"
#include "webrtc_session_pool.h"

namespace nx::webrtc {

Receiver::Receiver(Session* session): m_session(session)
{
}

/*virtual*/ Receiver::~Receiver()
{
    NX_ASSERT(!m_session->reader() || !m_session->reader()->isStarted());
}

/*virtual*/ bool Receiver::start(Ice* ice, Dtls* dtls)
{
    NX_ASSERT(ice);
    m_ice = ice;

    m_session->demuxer()->setSrtpEncryptionData(dtls->encryptionData());

    // TODO: I suppose FIR packets are not needed. Lets check in browser and remove it.
#if 0
    m_session->reader()->onKeyframeNeeded(
        [this]()
        {
            NX_DEBUG(this, "Send RTCP FIR into source");
            auto output = m_session->demuxer()->getRtcpFirPacket(
                m_session->tracks()->videoTrack().ssrc);
            if (output.size() != 0)
                m_ice->writePacket(output.data(), output.size(), /*foreground*/ false);
        });
#endif
    m_session->reader()->start();

    return true;
}

/*virtual*/ void Receiver::stop()
{
    m_session->reader()->stop();

    // Dangerous, object can be destroyed after this call.
    m_session->sessionPool()->drop(m_session->getLocalUfrag());
}

/*virtual*/ bool Receiver::onSrtp(std::vector<uint8_t> buffer)
{
    if (!m_session->demuxer()->processData((const char*) buffer.data(), buffer.size()))
    {
        NX_VERBOSE(this, "Failed to process SRTP data with size %1", buffer.size());
        return false;
    }
    while (true)
    {
        nx::Buffer output = m_session->demuxer()->getNextOutput();
        if (output.size() == 0)
            break;
        m_ice->writePacket(output.data(), output.size(), /*foreground*/ false);
    }
    while (true)
    {
        QnAbstractMediaDataPtr media = m_session->demuxer()->getNextFrame();
        if (!media)
            break;
        m_session->reader()->pushFrame(std::move(media));
    }
    return true;
}

/*virtual*/ void Receiver::onDataChannelString(const std::string& /*data*/, int /*streamId*/)
{
    // Nop.
}

/*virtual*/ void Receiver::onDataChannelBinary(const std::string& /*data*/, int /*streamId*/)
{
    // Nop.
}

Ice* Receiver::ice()
{
    NX_ASSERT(m_ice);
    return m_ice;
}

} // namespace nx::webrtc
