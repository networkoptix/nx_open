#include "transcode_media_encoder.h"

#include "stream_reader.h"
#include "transcode_stream_reader.h"

namespace nx {
namespace usb_cam {

TranscodeMediaEncoder::TranscodeMediaEncoder(
    nxpt::CommonRefManager* const parentRefManager,
    int encoderIndex,
    const CodecParameters& codecParams,
    const std::shared_ptr<Camera>& camera)
    :
    MediaEncoder(
        parentRefManager,
        encoderIndex,
        codecParams,
        camera)
{
    std::cout << "TranscodeMediaEncoder" << std::endl;
}

TranscodeMediaEncoder::~TranscodeMediaEncoder()
{
    std::cout << "~TranscodeMediaEncoder" << std::endl;
}

nxcip::StreamReader* TranscodeMediaEncoder::getLiveStreamReader()
{
    if (!m_streamReader)
    {
        std::unique_ptr<StreamReaderPrivate> transcoder = std::make_unique<TranscodeStreamReader>(
            m_encoderIndex,
            m_codecParams,
            m_camera);
        
        m_streamReader.reset(new StreamReader(
            &m_refManager,
            std::move(transcoder)));
    }

    m_streamReader->addRef();
    return m_streamReader.get();
}

} // namespace nx
} // namespace usb_cam