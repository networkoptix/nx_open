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
    std::cout << "NativeMediaEncoder" << std::endl;
}

NativeMediaEncoder::~NativeMediaEncoder()
{
    std::cout << "~NativeMediaEncoder" << std::endl;
}

int NativeMediaEncoder::getResolutionList(nxcip::ResolutionInfo * infoList, int * infoListCount) const
{
    auto list = m_camera->getResolutionList();
    if (list.empty())
    {
        *infoListCount = 0;
        return nxcip::NX_OTHER_ERROR;
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
            m_camera));
    }

    m_streamReader->addRef();
    return m_streamReader.get();
}


} // namespace nx
} // namespace usb_cam