#include "native_media_encoder.h"

#include <nx/utils/log/log.h>

#include "stream_reader.h"

namespace nx {
namespace usb_cam {

NativeMediaEncoder::NativeMediaEncoder(
    nxpt::CommonRefManager* const parentRefManager,
    int encoderIndex,
    const std::shared_ptr<Camera>& camera)
    :
    MediaEncoder(
        parentRefManager,
        encoderIndex,
        camera)
{
}

int NativeMediaEncoder::getResolutionList(nxcip::ResolutionInfo * infoList, int * infoListCount) const
{
    if (!m_camera->videoStream()->pluggedIn())
        return nxcip::NX_IO_ERROR;

    auto list = m_camera->resolutionList();
    if (list.empty())
    {
        *infoListCount = 0;
        return nxcip::NX_IO_ERROR;
    }

    fillResolutionList(list, infoList, infoListCount);
    return nxcip::NX_NO_ERROR;
}

nxcip::StreamReader* NativeMediaEncoder::getLiveStreamReader()
{
    if (!m_streamReader)
    {
        m_streamReader.reset(new StreamReader(
            &m_refManager,
            m_encoderIndex,
            m_codecParams,
            m_camera,
            shouldStreamTranscode()));
    }

    m_streamReader->addRef();
    return m_streamReader.get();
}

bool NativeMediaEncoder::shouldStreamTranscode() const
{
#if 1
    return m_codecParams.codecId == AV_CODEC_ID_NONE
        ||  m_codecParams.codecId == AV_CODEC_ID_MJPEG;
#else
    return m_codecParams.codecId == AV_CODEC_ID_NONE;
#endif
}

} // namespace nx
} // namespace usb_cam