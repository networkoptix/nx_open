#include "rpi_camera.h"

#include "camera_manager.h"
#include "stream_reader.h"
#include "media_encoder.h"


namespace rpi_cam
{
    DEFAULT_REF_COUNTER(MediaEncoder)

    MediaEncoder::MediaEncoder(std::shared_ptr<RPiCamera> camera, unsigned encoderNumber)
    :   m_refManager(this),
        m_camera(camera),
        m_encoderNumber(encoderNumber)
    {
        debug_print("MediaEncoder() %d\n", m_encoderNumber);
    }

    MediaEncoder::~MediaEncoder()
    {
        debug_print("~MediaEncoder() %d\n", m_encoderNumber);
    }

    void * MediaEncoder::queryInterface(const nxpl::NX_GUID& interfaceID)
    {
        if (interfaceID == nxcip::IID_CameraMediaEncoder3)
        {
            addRef();
            return static_cast<nxcip::CameraMediaEncoder3*>(this);
        }
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

    int MediaEncoder::getMediaUrl(char * ) const
    {
        return nxcip::NX_NOT_IMPLEMENTED;
    }

    int MediaEncoder::getResolutionList(nxcip::ResolutionInfo * infoList, int * infoListCount) const
    {
        std::shared_ptr<RPiCamera> camera = m_camera.lock();
        if (! infoList || ! camera)
            return nxcip::NX_OTHER_ERROR;

        unsigned num;
        const rpi_omx::VideoFromat * formats = camera->getVideoFormats(num, (bool)m_encoderNumber);

        for (unsigned i = 0; i < num; ++i)
        {
            infoList[i].resolution.width = formats[i].width;
            infoList[i].resolution.height = formats[i].height;
            infoList[i].maxFps = formats[i].framerate;
            if (m_encoderNumber)
                infoList[i].maxFps /= RPiCamera::IFRAME_PERIOD();

            debug_print("MediaEncoder::getResolution(): %d %dx%dx%d\n",
                   m_encoderNumber, infoList[i].resolution.width, infoList[i].resolution.height, (int)infoList[i].maxFps);
        }

        *infoListCount = num;
        return nxcip::NX_NO_ERROR;
    }

    int MediaEncoder::getMaxBitrate(int * maxBitrate) const
    {
        *maxBitrate = 10000;
        return nxcip::NX_NO_ERROR;
    }

    int MediaEncoder::setResolution(const nxcip::Resolution& )
    {
        return nxcip::NX_NOT_IMPLEMENTED;
    }

    int MediaEncoder::setFps(const float& , float * )
    {
        return nxcip::NX_NOT_IMPLEMENTED;
    }

    int MediaEncoder::setBitrate(int , int * )
    {
        return nxcip::NX_NOT_IMPLEMENTED;
    }

    int MediaEncoder::getAudioFormat(nxcip::AudioFormat * ) const
    {
        return nxcip::NX_NOT_IMPLEMENTED;
    }

    int MediaEncoder::getVideoFormat(nxcip::CompressionType * , nxcip::PixelFormat * ) const
    {
        return nxcip::NX_NOT_IMPLEMENTED;
    }

    nxcip::StreamReader * MediaEncoder::getLiveStreamReader()
    {
        std::shared_ptr<RPiCamera> camera = m_camera.lock();
        if (! camera || ! camera->isOK())
            return nullptr;

        StreamReader * stream = new StreamReader(camera, m_encoderNumber);
        return stream;
    }

    int MediaEncoder::getConfiguredLiveStreamReader(nxcip::LiveStreamConfig * config, nxcip::StreamReader ** reader)
    {
        if (reader)
        {
            std::shared_ptr<RPiCamera> camera = m_camera.lock();
            if (! camera || ! camera->isOK())
                return nxcip::NX_OTHER_ERROR;

            if (m_encoderNumber == 0)
            {
                if (config->framerate > 30.0f)
                    config->framerate = 30.0f;

                debug_print("try configure encoder: %dx%dx%d %d kbps\n",
                       config->width, config->height, (unsigned)config->framerate, config->bitrateKbps);

                camera->updateEncoder(config->width, config->height, (unsigned)config->framerate, config->bitrateKbps);
            }

            *reader = getLiveStreamReader();
            if (*reader)
                return nxcip::NX_NO_ERROR;
        }

        return nxcip::NX_OTHER_ERROR;
    }
}
