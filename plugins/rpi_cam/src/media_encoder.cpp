#include "rpi_camera.h"

#include "camera_manager.h"
#include "stream_reader.h"
#include "media_encoder.h"


namespace rpi_cam
{
    DEFAULT_REF_COUNTER(MediaEncoder)

    MediaEncoder::MediaEncoder(std::shared_ptr<RPiCamera> camera, unsigned encoderNumber, const nxcip::ResolutionInfo& res)
    :   m_refManager(this),
        m_camera(camera),
        m_encoderNumber(encoderNumber),
        m_resolution(res)
    {
        printf("[camera] MediaEncoder() %d\n", m_encoderNumber);
    }

    MediaEncoder::~MediaEncoder()
    {
        printf("[camera] ~MediaEncoder() %d\n", m_encoderNumber);
    }

    void * MediaEncoder::queryInterface(const nxpl::NX_GUID& interfaceID)
    {
        if (interfaceID == nxcip::IID_CameraMediaEncoder2)
        {
            addRef();
            return static_cast<nxcip::CameraMediaEncoder2*>(this);
        }
        if (interfaceID == nxcip::IID_CameraMediaEncoder)
        {
            addRef();
            return static_cast<nxcip::CameraMediaEncoder*>(this);
        }

        return nullptr;
    }

    int MediaEncoder::getMediaUrl(char * urlBuf) const
    {
        strcpy(urlBuf, RPiCamera::UID());
        return nxcip::NX_NO_ERROR;
    }

    int MediaEncoder::getResolutionList(nxcip::ResolutionInfo * infoList, int * infoListCount) const
    {
        printf("[camera] MediaEncoder::getResolution(): %dx%dx%d\n",
               m_resolution.resolution.width, m_resolution.resolution.height, (int)m_resolution.maxFps);

        infoList[0] = m_resolution;
        *infoListCount = 1;
        return nxcip::NX_NO_ERROR;
    }

    int MediaEncoder::getMaxBitrate(int * maxBitrate) const
    {
        *maxBitrate = 10000;
        return nxcip::NX_NO_ERROR;
    }

    int MediaEncoder::setResolution(const nxcip::Resolution& res)
    {
        return nxcip::NX_NO_ERROR;
    }

    int MediaEncoder::setFps(const float& fps, float * selectedFps)
    {
#if 0
        if (! m_camera && ! m_camera->isOK())
            return nxcip::NX_OTHER_ERROR;

        if (m_encoderNumber)
            return nxcip::NX_NO_ERROR;

        m_camera->config(0, fps);
        *selectedFps = fps; // TODO
#endif
        return nxcip::NX_NO_ERROR;
    }

    int MediaEncoder::setBitrate(int bitrateKbps, int * outBitrateKbps)
    {
#if 0
        if (! m_camera && ! m_camera->isOK())
            return nxcip::NX_OTHER_ERROR;

        if (m_encoderNumber)
            m_camera->config(0, 0, 0, bitrateKbps);
        else
            m_camera->config(0, 0, bitrateKbps, 0);
        *outBitrateKbps = bitrateKbps; // TODO
#endif
        return nxcip::NX_NO_ERROR;
    }

    int MediaEncoder::commit()
    {
        if (! m_camera && ! m_camera->isOK())
            return nxcip::NX_OTHER_ERROR;

        m_camera->reload();
        return nxcip::NX_NO_ERROR;
    }

    nxcip::StreamReader * MediaEncoder::getLiveStreamReader()
    {
        StreamReader * stream = new StreamReader(m_camera, m_encoderNumber);
        return stream;
    }

    int MediaEncoder::getAudioFormat(nxcip::AudioFormat * /*audioFormat*/) const
    {
        return nxcip::NX_UNSUPPORTED_CODEC;
    }
}
