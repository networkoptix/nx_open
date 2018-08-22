#include "native_media_encoder.h"

#include <nx/utils/log/log.h>

#include "stream_reader.h"

namespace nx {
namespace usb_cam {

NativeMediaEncoder::NativeMediaEncoder(
    nxpt::CommonRefManager* const parentRefManager,
    int encoderIndex,
    const ffmpeg::CodecParameters& codecParams,
    const std::shared_ptr<Camera>& camera)
    :
    MediaEncoder(
        parentRefManager,
        encoderIndex,
        codecParams,
        camera)
{
    std::cout << "NativeMediaEncoder" << std::endl;
}

NativeMediaEncoder::~NativeMediaEncoder()
{
    std::cout << "~NativeMediaEncoder" << std::endl;
}

nxcip::StreamReader* NativeMediaEncoder::getLiveStreamReader()
{
    if (!m_streamReader)
    {
        m_streamReader.reset(new StreamReader(
            &m_refManager,
            m_encoderIndex,
            m_codecParams,
            m_camera));
    }

    m_streamReader->addRef();
    return m_streamReader.get();
}


} // namespace nx
} // namespace usb_cam