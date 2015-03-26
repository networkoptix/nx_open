#include "camera_manager.h"
#include "stream_reader.h"
#include "media_encoder.h"

#include "discovery_manager.h"

namespace ite
{

    INIT_OBJECT_COUNTER(MediaEncoder)
    DEFAULT_REF_COUNTER(MediaEncoder)

#if 0
    unsigned int MediaEncoder::addRef()
    {
        unsigned count = m_refManager.addRef();
        return count;
    }

    unsigned int MediaEncoder::releaseRef()
    {
        unsigned count = m_refManager.releaseRef();
        return count;
    }
#endif

    MediaEncoder::MediaEncoder(CameraManager * const cameraManager, int encoderNumber, const nxcip::ResolutionInfo& res)
    :   m_refManager(this),
        m_cameraManager(cameraManager),
        m_encoderNumber(encoderNumber),
        m_resolution(res),
        m_fpsToSet(0.0f),
        m_bitrateToSet(0)
    {
    }

    MediaEncoder::~MediaEncoder()
    {
    }

    void* MediaEncoder::queryInterface( const nxpl::NX_GUID& interfaceID )
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
        if (interfaceID == nxpl::IID_PluginInterface)
        {
            addRef();
            return static_cast<nxpl::PluginInterface*>(this);
        }

        return nullptr;
    }

    int MediaEncoder::getMediaUrl( char* urlBuf ) const
    {
        strcpy( urlBuf, m_cameraManager->url() );
        return nxcip::NX_NO_ERROR;
    }

    int MediaEncoder::getResolutionList( nxcip::ResolutionInfo* infoList, int* infoListCount ) const
    {
        infoList[0] = m_resolution;
        *infoListCount = 1;

        return nxcip::NX_NO_ERROR;
    }

    int MediaEncoder::getMaxBitrate(int * maxBitrate) const
    {
        static const unsigned MAX_BITRATE_KBPS = 12000; // default value from videoEncConfig[0].targetBitrateLimit

        *maxBitrate = MAX_BITRATE_KBPS;
        return nxcip::NX_NO_ERROR;
    }

    int MediaEncoder::setResolution(const nxcip::Resolution& resolution)
    {
        if (resolution.height != m_resolution.resolution.height || resolution.width != m_resolution.resolution.width)
            return nxcip::NX_UNSUPPORTED_RESOLUTION;
        return nxcip::NX_NO_ERROR;
    }

    int MediaEncoder::setFps(const float& fps, float *)
    {
        m_fpsToSet = fps;
        return nxcip::NX_NO_ERROR;
        //return commit();
    }

    int MediaEncoder::setBitrate(int bitrateKbps, int * selectedBitrateKbps)
    {
        static const unsigned MIN_BITRATE_KBPS = 200;

        m_bitrateToSet = bitrateKbps;
        if (m_bitrateToSet < MIN_BITRATE_KBPS)
            m_bitrateToSet = MIN_BITRATE_KBPS;

        *selectedBitrateKbps = m_bitrateToSet;
        //return nxcip::NX_NO_ERROR;
        return commit();
    }

    int MediaEncoder::commit()
    {
        if (m_fpsToSet != 0.0f && m_bitrateToSet)
        {
            RxDevicePtr rxDev = m_cameraManager->rxDevice().lock();
            if (! rxDev)
                return nxcip::NX_OTHER_ERROR;

            TxDevicePtr txDev = rxDev->txDevice();
            if (txDev)
            {
                int width, height, bitrate;
                float fps;
                txDev->videoEncoderCfg(m_encoderNumber, width, height, fps, bitrate);
                if (bitrate == (int)m_bitrateToSet && fps == m_fpsToSet)
                {
                    m_fpsToSet = 0.0f;
                    m_bitrateToSet = 0;
                    return nxcip::NX_NO_ERROR;
                }
            }

            if (! rxDev->setEncoderParams(m_encoderNumber, m_fpsToSet, m_bitrateToSet))
                return nxcip::NX_INVALID_PARAM_VALUE;

            m_fpsToSet = 0.0f;
            m_bitrateToSet = 0;
            return nxcip::NX_NO_ERROR;
        }

        return nxcip::NX_NO_ERROR;
    }

    nxcip::StreamReader* MediaEncoder::getLiveStreamReader()
    {
        StreamReader * stream = new StreamReader(m_cameraManager, m_encoderNumber);
        return stream;
    }

    int MediaEncoder::getAudioFormat( nxcip::AudioFormat* /*audioFormat*/ ) const
    {
        return nxcip::NX_UNSUPPORTED_CODEC;
    }
}
